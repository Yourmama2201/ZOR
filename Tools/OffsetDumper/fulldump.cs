using System;
using System.IO;
using System.Runtime.InteropServices;

public class FullDump {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFile(string n, uint a, uint s, IntPtr sec, uint d, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool DeviceIoControl(IntPtr h, uint code, byte[] inb, uint ins, byte[] outb, uint outs, out uint ret, IntPtr ov);

    static IntPtr Open() { return CreateFile(@"\\.\ZOR", 0xC0000000u, 0, IntPtr.Zero, 3, 0, IntPtr.Zero); }

    static byte[] ReadDrv(IntPtr h, ulong pid, ulong addr, int sz) {
        var tmp = new byte[32 + sz];
        BitConverter.GetBytes(pid).CopyTo(tmp, 0);
        BitConverter.GetBytes(addr).CopyTo(tmp, 8);
        BitConverter.GetBytes(0L).CopyTo(tmp, 16);
        BitConverter.GetBytes((long)sz).CopyTo(tmp, 24);
        uint ret = 0;
        bool ok = DeviceIoControl(h, 0x222000u, tmp, (uint)tmp.Length, tmp, (uint)tmp.Length, out ret, IntPtr.Zero);
        if (ok && ret >= 32) { var r = new byte[sz]; Array.Copy(tmp, 32, r, 0, sz); return r; }
        return null;
    }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        string outFile = args[1];
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED err=" + Marshal.GetLastWin32Error()); return 1; }

        ulong baseAddr = ulong.Parse(args[2], System.Globalization.NumberStyles.HexNumber);
        var dos = ReadDrv(h, pid, baseAddr, 0x40);
        if (dos == null) { Console.Error.WriteLine("CANNOT READ DOS"); return 1; }
        int lfanew = BitConverter.ToInt32(dos, 0x3C);
        var nt = ReadDrv(h, pid, baseAddr + (ulong)lfanew, 0x200);
        if (nt == null) { Console.Error.WriteLine("CANNOT READ NT"); return 1; }
        uint sizeOfImage = BitConverter.ToUInt32(nt, 0x50);
        uint numSections = BitConverter.ToUInt16(nt, 0x6);
        Console.WriteLine("SizeOfImage=0x" + sizeOfImage.ToString("x") + " sections=" + numSections);

        // Section headers start right after optional header
        int optSize = BitConverter.ToUInt16(nt, 0x14);
        int sectStart = 4 + 20 + optSize;
        var headers = new StreamWriter(outFile + ".sections.txt");
        var full = new byte[sizeOfImage];
        // Read headers region
        var head = ReadDrv(h, pid, baseAddr, Math.Min(0x1000, (int)sizeOfImage));
        if (head != null) Array.Copy(head, 0, full, 0, Math.Min(head.Length, full.Length));

        ulong va = baseAddr + (ulong)lfanew + (ulong)sectStart;
        for (int i = 0; i < numSections; i++) {
            var sh = ReadDrv(h, pid, va + (ulong)(i * 40), 40);
            if (sh == null) continue;
            string name = System.Text.Encoding.ASCII.GetString(sh, 0, 8).TrimEnd('\0');
            uint vsize = BitConverter.ToUInt32(sh, 8);
            uint vaddr = BitConverter.ToUInt32(sh, 12);
            uint rawSize = BitConverter.ToUInt32(sh, 16);
            uint rawPtr = BitConverter.ToUInt32(sh, 20);
            uint chars = BitConverter.ToUInt32(sh, 36);
            headers.WriteLine("SECT " + name + " VA=0x" + vaddr.ToString("x") + " VSize=0x" + vsize.ToString("x") +
                " RawPtr=0x" + rawPtr.ToString("x") + " RawSize=0x" + rawSize.ToString("x") +
                " RVA=0x" + (baseAddr + vaddr).ToString("x") + " Chars=0x" + chars.ToString("x"));
            Console.WriteLine("SECT " + name + " VA=0x" + vaddr.ToString("x") + " Size=0x" + vsize.ToString("x"));
        }
        headers.Close();

        // Dump the full image (every page)
        using (var fs = new FileStream(outFile, FileMode.Create, FileAccess.Write)) {
            const int CHUNK = 0x10000;
            int okChunks = 0, tot = 0;
            for (ulong off = 0; off < sizeOfImage; off += CHUNK) {
                int sz = (int)Math.Min(CHUNK, sizeOfImage - off);
                var b = ReadDrv(h, pid, baseAddr + off, sz);
                if (b != null) { fs.Write(b, 0, sz); okChunks++; }
                else { fs.Write(new byte[sz], 0, sz); }
                tot++;
            }
            Console.WriteLine("DUMPED " + outFile + " " + sizeOfImage + " bytes, ok=" + okChunks + "/" + tot);
        }
        return 0;
    }
}

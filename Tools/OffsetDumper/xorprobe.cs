using System;
using System.Runtime.InteropServices;

public class XorProbe {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFile(string n, uint a, uint s, IntPtr sec, uint d, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool DeviceIoControl(IntPtr h, uint code, byte[] inb, uint ins, byte[] outb, uint outs, out uint ret, IntPtr ov);
    static IntPtr Open() { return CreateFile(@"\\.\ZOR", 0xC0000000u, 0, IntPtr.Zero, 3, 0, IntPtr.Zero); }
    static byte[] Read(IntPtr h, uint pid, ulong addr, int sz) {
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
    static ulong Ptr(byte[] b, int off) { return BitConverter.ToUInt64(b, off); }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        // read the raw DISTRIBUTE slot entries as 16-byte pairs {p0, p1}, test p0^p1 readability
        ulong dist = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8), 0);
        Console.WriteLine("DISTRIBUTE slot value = 0x{0:x} (mod+0x{1:x})", dist, dist - baseAddr);
        for (int i = 0; i < 12; i++) {
            ulong a = dist + (ulong)(i * 0x10);
            var b = Read(h, pid, a, 0x10);
            if (b == null) { Console.WriteLine("  [{0}] read fail", i); continue; }
            ulong p0 = Ptr(b, 0), p1 = Ptr(b, 8);
            ulong x = p0 ^ p1;
            var rb = Read(h, pid, x, 0x20);
            bool readable = rb != null;
            string tag = readable ? "READABLE" : "unreadable";
            Console.WriteLine("  [{0,2}] p0=0x{1:x} p1=0x{2:x} x=0x{3:x} -> {4}", i, p0, p1, x, tag);
        }
        // also test raw p1 readability (odd qwords from the original array)
        Console.WriteLine("\nodd-qword entries (el + i*8 for i odd):");
        for (int i = 0; i < 12; i++) {
            ulong a = dist + (ulong)(i * 0x8);
            var b = Read(h, pid, a, 8);
            if (b == null) continue;
            ulong v = Ptr(b, 0);
            var rb = Read(h, pid, v, 0x20);
            string tag = rb != null ? "READABLE" : "unreadable";
            Console.WriteLine("  [o{0,2}] 0x{1:x} -> {2}", i, v, tag);
        }
        return 0;
    }
}

using System;
using System.Runtime.InteropServices;

public class Probe3 {
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
    static float Flt(byte[] b, int off) { return BitConverter.ToSingle(b, off); }

    static void HexDump(IntPtr h, uint pid, ulong baseAddr, ulong addr, int len, string label) {
        var b = Read(h, pid, addr, len);
        if (b == null) { Console.WriteLine("{0} @0x{1:x}: READ FAIL", label, addr); return; }
        Console.WriteLine("=== {0} @0x{1:x} (mod+0x{2:x}) ===", label, addr, addr - baseAddr);
        for (int off = 0; off < len; off += 16) {
            var sb = new System.Text.StringBuilder();
            sb.AppendFormat("{0,6:x}  ", off);
            for (int i = 0; i < 16; i++) sb.AppendFormat("{0:x2} ", (off + i < len) ? b[off + i] : 0);
            sb.Append(" | ");
            for (int i = 0; i < 16 && off + i < len; i++) {
                char c = (char)b[off + i];
                sb.Append((c >= 32 && c < 127) ? c : '.');
            }
            Console.WriteLine(sb);
        }
        // interpret as pointers
        for (int off = 0; off + 8 <= len; off += 8) {
            ulong v = Ptr(b, off);
            string tag = "";
            if (v >= baseAddr && v < baseAddr + 0x2591c200) tag = " [mod+0x" + (v - baseAddr).ToString("x") + "]";
            else if (v == 0) tag = " [null]";
            else if (v >= 0x00007ff000000000 && v < 0x0000800000000000) tag = " [ntdll/other]";
            else if (v >= 0x0000000000010000 && v < 0x00007ff000000000) tag = " [heap]";
            if (v != 0) Console.WriteLine("  +0x{0:x}: 0x{1:x}{2}  f0={3} f1={4}", off, v, tag, Flt(b, off), Flt(b, off + 4));
        }
    }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        ulong dist = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8), 0);
        Console.WriteLine("DISTRIBUTE ptr=0x{0:x} (mod+0x{1:x})", dist, dist - baseAddr);

        // Dump the list header itself
        HexDump(h, pid, baseAddr, dist, 0x60, "DISTRIBUTE header");
        // Dump an entity
        ulong e0 = Ptr(Read(h, pid, dist, 8), 0);
        if (e0 >= baseAddr && e0 < baseAddr + 0x2591c200)
            HexDump(h, pid, baseAddr, e0, 0x80, "entity[0] head");
        return 0;
    }
}

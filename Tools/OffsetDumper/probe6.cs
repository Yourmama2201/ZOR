using System;
using System.Runtime.InteropServices;

public class Probe6 {
    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
    static extern IntPtr CreateFile(string n, uint a, uint s, IntPtr sec, uint d, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError=true)]
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
    static bool Finite(float f) { return !float.IsNaN(f) && !float.IsInfinity(f); }
    static void Dump(IntPtr h, uint pid, ulong baseAddr, ulong addr, int len, string label) {
        var b = Read(h, pid, addr, len);
        if (b == null) { Console.WriteLine("{0} @0x{1:x}: READ FAIL", label, addr); return; }
        Console.WriteLine("=== {0} @0x{1:x} (mod+0x{2:x}) ===", label, addr, addr-baseAddr);
        for (int off = 0; off+8 <= len; off += 8) {
            ulong v = BitConverter.ToUInt64(b, off);
            float f0 = BitConverter.ToSingle(b, off), f1 = BitConverter.ToSingle(b, off+4);
            string tag = "";
            if (v >= baseAddr && v < baseAddr+0x2591c200) tag = " mod+"+(v-baseAddr).ToString("x");
            else if (v==0) tag = " null";
            else if (v < 0x10000) tag = " small";
            string fs = Finite(f0)&&Finite(f1) ? string.Format(" ({0:0.#},{1:0.#})", f0, f1) : "";
            Console.WriteLine("  +0x{0:X4}: 0x{1:x16}{2}{3}", off, v, tag, fs);
        }
    }
    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64()==-1) { Console.Error.WriteLine("OPEN FAILED"); return 1; }
        Dump(h, pid, baseAddr, baseAddr+0xa8c9ad8, 0x200, "node 0xa8c9ad8");
        Dump(h, pid, baseAddr, baseAddr+0xa8d2ff0, 0x100, "node 0xa8d2ff0");
        Dump(h, pid, baseAddr, baseAddr+0xa8e8c80, 0x100, "node 0xa8e8c80");
        return 0;
    }
}

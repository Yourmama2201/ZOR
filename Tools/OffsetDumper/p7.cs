using System;
using System.Runtime.InteropServices;
public class P7 {
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
    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED"); return 1; }
        string[] rvas = new string[] { "cd1031c", "c832580", "cd24adc", "cd1fb30" };
        foreach (string a in rvas) {
            ulong rva = ulong.Parse(a, System.Globalization.NumberStyles.HexNumber);
            var b = Read(h, pid, baseAddr + rva, 0x60);
            if (b == null) { Console.WriteLine("mod+" + a + ": FAIL"); continue; }
            Console.WriteLine("=== mod+" + a + " ===");
            for (int i = 0; i < 0x60; i += 4) {
                float f = BitConverter.ToSingle(b, i);
                Console.WriteLine("  +0x" + i.ToString("X2") + " = " + f.ToString("0.####"));
            }
        }
        return 0;
    }
}

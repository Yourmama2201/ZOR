using System;
using System.Runtime.InteropServices;

public class PtrScan {
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
        // targets (module-relative)
        ulong[] targets = new ulong[] {
            0xc832580, 0xcd1031c, 0xcd1fb30, 0xcd24adc
        };
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        int win = 0x40000;
        // scan entire module image for pointers that fall within +-0x1000 of a target, or equal
        ulong start = baseAddr, end = baseAddr + 0x2591c200;
        for (ulong off = 0; off < end - start; off += (ulong)win) {
            ulong s = start + off;
            byte[] b = Read(h, pid, s, win);
            if (b == null) continue;
            for (int i = 0; i + 8 <= win; i += 8) {
                ulong v = Ptr(b, i);
                if (v < baseAddr + 0x1000 || v >= baseAddr + 0x2591c200) continue;
                ulong rva = v - baseAddr;
                foreach (ulong t in targets) {
                    if (rva >= t - 0x20 && rva <= t + 0x20) {
                        Console.WriteLine("PTR @ mod+0x{0:x} -> mod+0x{1:x} (near target 0x{2:x})", (s - baseAddr) + (ulong)i, rva, t);
                    }
                }
            }
        }
        Console.WriteLine("done");
        return 0;
    }
}

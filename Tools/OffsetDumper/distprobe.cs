using System;
using System.Runtime.InteropServices;

public class DistProbe {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFile(string n, uint a, uint s, IntPtr sec, uint d, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool DeviceIoControl(IntPtr h, uint code, byte[] inb, uint ins, byte[] outb, uint outs, out uint ret, IntPtr ov);

    static IntPtr Open() { return CreateFile(@"\\.\ZOR", 0xC0000000u, 0, IntPtr.Zero, 3, 0, IntPtr.Zero); }

    static byte[] Read(IntPtr h, ulong pid, ulong addr, int sz) {
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

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        ulong dist = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8), 0);
        ulong distOff = dist - baseAddr;
        Console.WriteLine("DISTRIBUTE slot @+0xA88DD10 = 0x{0:x} (mod+0x{1:x})", dist, distOff);

        // Dump the 16-byte stride table - first 16 entries, full 16 bytes each
        Console.WriteLine("=== 16-byte stride table at dist ===");
        for (int i = 0; i < 16; i++) {
            var b = Read(h, pid, dist + (ulong)(i * 16), 16);
            if (b == null) { Console.WriteLine("  [{0}] READ FAIL", i); continue; }
            ulong p0 = Ptr(b, 0), p1 = Ptr(b, 8);
            float f0 = Flt(b, 0), f1 = Flt(b, 4), f2 = Flt(b, 8), f3 = Flt(b, 12);
            string t0 = (p0 >= baseAddr && p0 < baseAddr + 0x2591c200) ? " [mod+0x" + (p0 - baseAddr).ToString("x") + "]" : "";
            Console.WriteLine("  [{0}] p0=0x{1:x}{2}  p1=0x{3:x}   f={4} {5} {6} {7}",
                i, p0, t0, p1, f0, f1, f2, f3);
        }

        // For each module pointer in the table, read a bit of what it points to
        Console.WriteLine("=== contents of module pointers (even qwords) ===");
        for (int i = 0; i < 16; i++) {
            var b = Read(h, pid, dist + (ulong)(i * 16), 8);
            if (b == null) continue;
            ulong p0 = Ptr(b, 0);
            if (p0 < baseAddr || p0 >= baseAddr + 0x2591c200) continue;
            var cb = Read(h, pid, p0, 48);
            if (cb == null) { Console.WriteLine("  [{0}] ptr=+0x{1:x} READ FAIL", i, p0 - baseAddr); continue; }
            Console.Write("  [{0}] ptr=+0x{1:x}:", i, p0 - baseAddr);
            for (int k = 0; k < 48; k += 8) Console.Write(" 0x{0:x}", Ptr(cb, k));
            Console.WriteLine();
        }

        // XOR candidate: odd qword (p1) vs even (p0)
        Console.WriteLine("=== p0 XOR p1 (encryption key test) ===");
        for (int i = 0; i < 8; i++) {
            var b = Read(h, pid, dist + (ulong)(i * 16), 16);
            if (b == null) continue;
            ulong p0 = Ptr(b, 0), p1 = Ptr(b, 8);
            ulong x = p0 ^ p1;
            var xb = Read(h, pid, x, 8);
            string res = xb == null ? "UNREADABLE" : ("0x" + Ptr(xb, 0).ToString("x"));
            Console.WriteLine("  [{0}] p0^p1=0x{1:x} -> {2}", i, x, res);
        }

        // Check whether entries are a singly linked list: does even qword point to next entry?
        Console.WriteLine("=== linked-list test: does p0[i] point near dist+((i+1)*16)? ===");
        for (int i = 0; i < 6; i++) {
            ulong expect = dist + (ulong)((i + 1) * 16);
            ulong p0 = Ptr(Read(h, pid, dist + (ulong)(i * 16), 8), 0);
            Console.WriteLine("  [{0}] p0=0x{1:x}  next-entry=0x{2:x}  diff={3}",
                i, p0, expect, p0 == expect ? "SAME" : "diff");
        }

        // Dump the full 64 bytes at the dist slot itself and its neighbors
        Console.WriteLine("=== dist slot region raw ===");
        for (int i = -4; i <= 4; i++) {
            ulong a = dist + (ulong)(i * 16);
            var b = Read(h, pid, a, 16);
            if (b == null) continue;
            ulong p0 = Ptr(b, 0), p1 = Ptr(b, 8);
            Console.WriteLine("  +0x{0:x} [+{1}]  p0=0x{2:x} p1=0x{3:x}",
                a - baseAddr, i, p0, p1);
        }
        return 0;
    }
}

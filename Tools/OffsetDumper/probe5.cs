using System;
using System.Runtime.InteropServices;

public class Probe5 {
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
    static bool Finite(float f) { return !float.IsNaN(f) && !float.IsInfinity(f); }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        // Walk DISTRIBUTE circular list. Head = value of slot. Follow +0 next.
        ulong head = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8), 0);
        Console.WriteLine("DISTRIBUTE slot = 0x{0:x} (mod+0x{1:x})", head, head - baseAddr);
        ulong node = head;
        for (int i = 0; i < 40; i++) {
            var nb = Read(h, pid, node, 0x40);
            if (nb == null) { Console.WriteLine("  [{0}] read fail @0x{1:x}", i, node); break; }
            ulong n0 = Ptr(nb, 0);
            // look for floats
            float f0 = BitConverter.ToSingle(nb, 0x8), f1 = BitConverter.ToSingle(nb, 0xC);
            float f2 = BitConverter.ToSingle(nb, 0x10), f3 = BitConverter.ToSingle(nb, 0x14);
            bool posLike = Finite(f0) && Finite(f1) && Finite(f2) && Math.Abs(f0) < 30000 && Math.Abs(f1) < 30000 && Math.Abs(f2) < 30000;
            Console.WriteLine("  [{0,2}] node@0x{1:x} (mod+0x{2:x}) next=0x{3:x}  floats@8=({4:0.#},{5:0.#}) @10=({6:0.#},{7:0.#}) {8}", i, node, node - baseAddr, n0, f0, f1, f2, f3, posLike ? "POS?" : "");
            if (n0 == head) { Console.WriteLine("  -> back to head (circular)"); break; }
            if (n0 < baseAddr || n0 >= baseAddr + 0x2591c200) { Console.WriteLine("  -> non-module next, stop"); break; }
            if (i > 0 && n0 == node) { Console.WriteLine("  -> self-loop, stop"); break; }
            node = n0;
        }
        return 0;
    }
}

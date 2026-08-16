using System;
using System.Runtime.InteropServices;

public class Probe4 {
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

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        ulong dist = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8), 0);
        Console.WriteLine("DISTRIBUTE slot @ base+0xA88DD10 = 0x{0:x} (mod+0x{1:x})", dist, dist - baseAddr);

        // Walk the circular list: node layout { next, prev_or_data, ... }
        // Try interpretation A: nodes are 0x10 apart? read node[0] -> next
        // Let's read 8 nodes: node = *(node + 0)
        ulong node = dist;
        for (int i = 0; i < 8; i++) {
            var nb = Read(h, pid, node, 0x40);
            if (nb == null) { Console.WriteLine("  read fail @0x{0:x}", node); break; }
            ulong n0 = Ptr(nb, 0);
            Console.WriteLine("  node[{0}] @0x{1:x} (mod+0x{2:x}): n0=0x{3:x} (mod+0x{4:x})", i, node, node - baseAddr, n0, n0 - baseAddr);
            if (n0 == dist) { Console.WriteLine("   -> back to head, circular list"); break; }
            if (n0 < baseAddr || n0 >= baseAddr + 0x2591c200) { Console.WriteLine("   -> non-module, stop"); break; }
            node = n0;
        }

        // Also try interpretation B: the slot is an ARRAY of {ptr, data} pairs (16-byte stride)
        Console.WriteLine("\nInterpretation B: 16-byte stride array at slot:");
        for (int i = 0; i < 12; i++) {
            ulong a = dist + (ulong)(i * 0x10);
            var b = Read(h, pid, a, 0x10);
            if (b == null) break;
            ulong p = Ptr(b, 0); ulong d = Ptr(b, 8);
            string tag = "";
            if (p >= baseAddr && p < baseAddr + 0x2591c200) tag = " mod+" + (p - baseAddr).ToString("x");
            else if (p == 0) tag = " null";
            Console.WriteLine("  [{0}] p=0x{1:x}{2}  data=0x{3:x}", i, p, tag, d);
        }
        return 0;
    }
}

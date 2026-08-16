using System;
using System.Runtime.InteropServices;

public class DecryptProbe {
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

    static ulong RotL(ulong x, int n) { return (x << n) | (x >> (64 - n)); }
    static ulong RotR(ulong x, int n) { return (x >> n) | (x << (64 - n)); }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        ulong dist = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8), 0);
        Console.WriteLine("DISTRIBUTE = 0x{0:x} (mod+0x{1:x})", dist, dist - baseAddr);

        // Test candidate decryptions on each entry, attempt to read the result
        string[] labels = { "p0^p1", "p0+p1", "p1-p0", "p0-p1", "~p0", "~p1",
                            "rol(p0,32)", "ror(p0,32)", "p0^rol(p1,13)", "p1^rol(p0,17)",
                            "p0+base", "p0^base" };

        for (int i = 0; i < 8; i++) {
            var b = Read(h, pid, dist + (ulong)(i * 16), 16);
            if (b == null) continue;
            ulong p0 = Ptr(b, 0), p1 = Ptr(b, 8);

            ulong[] cands = {
                p0 ^ p1, p0 + p1, p1 - p0, p0 - p1, ~p0, ~p1,
                RotL(p0, 32), RotR(p0, 32), p0 ^ RotL(p1, 13), p1 ^ RotL(p0, 17),
                p0 + baseAddr, p0 ^ baseAddr
            };

            Console.WriteLine("-- entry [{0}] p0=0x{1:x} p1=0x{2:x} --", i, p0, p1);
            for (int k = 0; k < cands.Length; k++) {
                ulong c = cands[k];
                var cb = Read(h, pid, c, 8);
                string tag = "";
                if (cb != null) {
                    ulong cv = Ptr(cb, 0);
                    string t2 = (cv >= baseAddr && cv < baseAddr + 0x2591c200) ? " [->mod+0x" + (cv - baseAddr).ToString("x") + "]" : "";
                    tag = "READABLE val=0x" + cv.ToString("x") + t2;
                } else {
                    tag = "unreadable";
                }
                bool canonical = c < 0x800000000000;
                Console.WriteLine("  {0,-14} = 0x{1:x16} {2} {3}", labels[k], c, canonical ? "" : "(noncanon)", tag);
            }
        }
        return 0;
    }
}

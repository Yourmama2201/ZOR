using System;
using System.Runtime.InteropServices;

public class Probe2 {
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
    static void Dump(IntPtr h, uint pid, ulong baseAddr, ulong addr, string label) {
        var b = Read(h, pid, addr, 64);
        if (b == null) { Console.WriteLine("{0}: READ FAIL", label); return; }
        var sb = new System.Text.StringBuilder();
        for (int i = 0; i < 64; i++) sb.AppendFormat("{0:x2} ", b[i]);
        Console.WriteLine("{0} @0x{1:x}:\n  {2}", label, addr, sb);
        for (int off = 0; off < 64; off += 8) {
            ulong v = Ptr(b, off);
            string tag = (v >= baseAddr && v < baseAddr + 0x2591c200) ? " [mod+0x" + (v - baseAddr).ToString("x") + "]" : "";
            if (v >= 0x00007ff000000000 && v < 0x0000800000000000 && (v < baseAddr || v >= baseAddr + 0x2591c200)) tag = " [heap/other]";
            if (v != 0) Console.WriteLine("    +0x{0:x2}: 0x{1:x}{2} f={3}", off, v, tag, Flt(b, off));
        }
    }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        Console.WriteLine("=== ENTITY STRUCTS from DISTRIBUTE ===");
        ulong dist = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8), 0);
        Console.WriteLine("dist=0x{0:x} (mod+0x{1:x})", dist, dist - baseAddr);
        for (int i = 0; i < 12; i++) {
            ulong e = Ptr(Read(h, pid, dist + (ulong)(i * 8), 8), 0);
            if (e < baseAddr || e >= baseAddr + 0x2591c200) continue;
            Console.WriteLine("--- entity[{0}] = 0x{1:x} (mod+0x{2:x}) ---", i, e, e - baseAddr);
            // check key fields at old offsets
            var b = Read(h, pid, e, 0x40);
            if (b == null) continue;
            int valid = Read(h, pid, e + 0x1236, 1)[0];
            int team = Read(h, pid, e + 0x2F1, 1)[0];
            float hp = Flt(Read(h, pid, e + 0x520, 8), 0);
            var posb = Read(h, pid, e + 0x1740, 16);
            float px = posb != null ? Flt(posb, 0) : 0;
            float py = posb != null ? Flt(posb, 4) : 0;
            float pz = posb != null ? Flt(posb, 8) : 0;
            var nameb = Read(h, pid, e + 0x800, 32);
            string name = nameb != null ? System.Text.Encoding.ASCII.GetString(nameb).TrimEnd('\0', ' ') : "";
            Console.WriteLine("    valid@1236={0} team@2F1={1} hp@520={2} pos@1740=({3},{4},{5}) name@800='{6}'",
                valid, team, hp, px, py, pz, name);
        }

        Console.WriteLine("=== BONE BASE chain ===");
        ulong boneRaw = Ptr(Read(h, pid, baseAddr + 0xCB97E48, 8), 0);
        Console.WriteLine("boneRaw=0x{0:x}", boneRaw);

        Console.WriteLine("=== STRING SEARCH: version/build strings ===");
        // Look for .rdata strings near buildinf section
        var rdata = Read(h, pid, baseAddr + 0x84b4000, 0x1000);
        if (rdata != null) {
            string s = System.Text.Encoding.ASCII.GetString(rdata);
            for (int i = 0; i + 8 < s.Length; i++) {
                if ((s[i] == 'v' || s[i] == 'V') && (s[i+1] == '1' || s[i+1] == '2') && char.IsDigit(s[i+1])) {
                    string t = s.Substring(i, 24).Trim();
                    if (t.IndexOf('.') >= 0) Console.WriteLine("  str @rdata+0x{0:x}: '{1}'", i, t);
                }
            }
        }
        return 0;
    }
}

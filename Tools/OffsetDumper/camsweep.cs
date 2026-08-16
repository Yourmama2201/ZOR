using System;
using System.Runtime.InteropServices;

public class CamSweep {
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
    static bool Finite(float f) { return !float.IsNaN(f) && !float.IsInfinity(f); }

    static void Scan(IntPtr h, uint pid, ulong baseAddr, ulong start, ulong end, string label) {
        int chunk = 0x10000;
        int found = 0;
        for (ulong off = 0; off < end - start; off += (ulong)chunk) {
            int want = (int)Math.Min((ulong)chunk, end - start - off);
            var b = Read(h, pid, start + off, want);
            if (b == null) continue;
            for (int i = 0; i + 64 <= want; i += 4) {
                float[] m = new float[16];
                for (int k = 0; k < 16; k++) m[k] = BitConverter.ToSingle(b, i + k * 4);
                bool allFin = true; for (int q=0;q<16;q++) if (!Finite(m[q])) { allFin=false; break; } if (!allFin) continue;
                if (m[15] != 0 && m[15] != 1) continue;
                // rotation part must be near-identity-free (camera has actual rotation in menu)
                float a0 = m[0], a1 = m[1], a2 = m[2], b0 = m[4], b1 = m[5], b2 = m[6], c0 = m[8], c1 = m[9], c2 = m[10];
                float l0 = (float)Math.Sqrt(a0*a0+b0*b0+c0*c0);
                float l1 = (float)Math.Sqrt(a1*a1+b1*b1+c1*c1);
                float l2 = (float)Math.Sqrt(a2*a2+b2*b2+c2*c2);
                if (Math.Abs(l0-1) > 0.1 || Math.Abs(l1-1) > 0.1 || Math.Abs(l2-1) > 0.1) continue;
                // require non-identity rotation (some rotation present)
                float identDist = Math.Abs(a0-1)+Math.Abs(a1)+Math.Abs(a2)+Math.Abs(b0)+Math.Abs(b1-1)+Math.Abs(b2)+Math.Abs(c0)+Math.Abs(c1)+Math.Abs(c2-1);
                if (identDist < 0.3) continue;
                // orthogonality
                float d01 = Math.Abs(a0*b0+a1*b1+a2*b2), d12 = Math.Abs(b0*c0+b1*c1+b2*c2), d20 = Math.Abs(c0*a0+c1*a1+c2*a2);
                if (d01 > 0.3 || d12 > 0.3 || d20 > 0.3) continue;
                // translation sane
                float tx = m[12], ty = m[13], tz = m[14];
                if (Math.Abs(tx) > 200000 || Math.Abs(ty) > 200000 || Math.Abs(tz) > 200000) continue;
                ulong addr = start + off + (ulong)i;
                Console.WriteLine("{0} VM? @ 0x{1:x} (mod+0x{2:x}) pos=({3:0.##},{4:0.##},{5:0.##}) rot0=({6:0.###},{7:0.###},{8:0.###}) rot1=({9:0.###},{10:0.###},{11:0.###})", label, addr, addr-baseAddr, tx, ty, tz, a0,b0,c0, a1,b1,c1);
                found++;
                if (found >= 40) return;
            }
        }
        if (found == 0) Console.WriteLine("{0}: none found", label);
    }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }
        Scan(h, pid, baseAddr, baseAddr + 0x93a9000, baseAddr + 0x12681000, ".data");
        Scan(h, pid, baseAddr, baseAddr + 0x84b4000, baseAddr + 0x93a9000, ".rdata");
        return 0;
    }
}


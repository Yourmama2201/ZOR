using System;
using System.Runtime.InteropServices;

public class DiffScan {
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

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        int win = 0x40000;
        // scan .data (writable, holds camera). Read twice with delay to find changing matrices.
        ulong start = baseAddr + 0x93a9000, end = baseAddr + 0x12681000;
        int found = 0;
        for (ulong off = 0; off < end - start; off += (ulong)win) {
            ulong s = start + off;
            byte[] a = Read(h, pid, s, win);
            System.Threading.Thread.Sleep(400);
            byte[] b = Read(h, pid, s, win);
            if (a == null || b == null) continue;
            for (int i = 0; i + 64 <= win - 8; i += 4) {
                // matrix changed significantly in translation? check position floats diff
                float a12 = BitConverter.ToSingle(a, i + 48), b12 = BitConverter.ToSingle(b, i + 48);
                float a13 = BitConverter.ToSingle(a, i + 52), b13 = BitConverter.ToSingle(b, i + 52);
                float a14 = BitConverter.ToSingle(a, i + 56), b14 = BitConverter.ToSingle(b, i + 56);
                if (!Finite(a12) || !Finite(b12) || !Finite(a13) || !Finite(b13) || !Finite(a14) || !Finite(b14)) continue;
                float d = Math.Abs(a12 - b12) + Math.Abs(a13 - b13) + Math.Abs(a14 - b14);
                if (d < 0.001f || d > 50000f) continue;
                // check rotation part looks like orthonormal matrix
                float[] m = new float[16];
                for (int k = 0; k < 16; k++) m[k] = BitConverter.ToSingle(a, i + k * 4);
                float l0 = (float)Math.Sqrt(m[0]*m[0]+m[4]*m[4]+m[8]*m[8]);
                float l1 = (float)Math.Sqrt(m[1]*m[1]+m[5]*m[5]+m[9]*m[9]);
                float l2 = (float)Math.Sqrt(m[2]*m[2]+m[6]*m[6]+m[10]*m[10]);
                if (Math.Abs(l0-1) > 0.2 || Math.Abs(l1-1) > 0.2 || Math.Abs(l2-1) > 0.2) continue;
                float d01 = Math.Abs(m[0]*m[1]+m[4]*m[5]+m[8]*m[9]);
                if (d01 > 0.4) continue;
                ulong addr = s + (ulong)i;
                Console.WriteLine("CHANGING 4x4 @ mod+0x{0:x} pos1=({1:0.##},{2:0.##},{3:0.##}) -> pos2=({4:0.##},{5:0.##},{6:0.##})", addr - baseAddr, a12, a13, a14, b12, b13, b14);
                if (++found >= 40) return 0;
            }
        }
        if (found == 0) Console.WriteLine("no changing orthonormal matrices found");
        return 0;
    }
}

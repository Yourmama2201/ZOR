using System;
using System.Runtime.InteropServices;

public class VmScan {
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

    // scan a memory range for a 4x4 view matrix: orthonormal rows, finite, translation sane
    static void ScanRange(IntPtr h, uint pid, ulong baseAddr, ulong start, ulong end, string label) {
        ulong sz = end - start;
        int chunk = 0x10000;
        byte[] buf = new byte[chunk];
        int found = 0;
        for (ulong off = 0; off < sz; off += (ulong)chunk) {
            int want = (int)Math.Min((ulong)chunk, sz - off);
            var b = Read(h, pid, start + off, want);
            if (b == null) continue;
            for (int i = 0; i + 64 <= want; i += 4) {
                float[] m = new float[16];
                for (int k = 0; k < 16; k++) m[k] = BitConverter.ToSingle(b, i + k * 4);
                if (!Finite(m[0]) || !Finite(m[5]) || !Finite(m[10]) || !Finite(m[12]) || !Finite(m[13]) || !Finite(m[14])) continue;
                if (m[15] != 0 && m[15] != 1) continue;
                // column orthonormal check (view matrix has orthonormal rotation part)
                Vec3 c0 = new Vec3(m[0], m[4], m[8]);
                Vec3 c1 = new Vec3(m[1], m[5], m[9]);
                Vec3 c2 = new Vec3(m[2], m[6], m[10]);
                float l0 = c0.Len(), l1 = c1.Len(), l2 = c2.Len();
                if (Math.Abs(l0 - 1.0f) > 0.05f || Math.Abs(l1 - 1.0f) > 0.05f || Math.Abs(l2 - 1.0f) > 0.05f) continue;
                float d01 = Math.Abs(c0.Dot(c1)), d02 = Math.Abs(c0.Dot(c2)), d12 = Math.Abs(c1.Dot(c2));
                if (d01 > 0.1f || d02 > 0.1f || d12 > 0.1f) continue;
                // translation should be within world bounds (map coords up to ~30k)
                float tx = m[12], ty = m[13], tz = m[14];
                if (Math.Abs(tx) > 200000 || Math.Abs(ty) > 200000 || Math.Abs(tz) > 200000) continue;
                // skip identity-ish (no rotation) - camera should have some rotation
                float rot = c0.Dot(new Vec3(1, 0, 0)) + c1.Dot(new Vec3(0, 1, 0));
                ulong addr = start + off + (ulong)i;
                Console.WriteLine("{0} 4x4 @ 0x{1:x} (mod+0x{2:x})  pos=({3:0.##},{4:0.##},{5:0.##}) row0=({6:0.###},{7:0.###},{8:0.###},{9:0.###})", label, addr, addr - baseAddr, tx, ty, tz, m[0], m[1], m[2], m[3]);
                found++;
                if (found >= 30) { Console.WriteLine("  (cap reached)"); return; }
            }
        }
        if (found == 0) Console.WriteLine("{0}: no orthonormal finite 4x4 found in range", label);
    }

    struct Vec3 { public float x, y, z; public Vec3(float a, float b, float c) { x = a; y = b; z = c; } public float Len() { return (float)Math.Sqrt(x * x + y * y + z * z); } public float Dot(Vec3 o) { return x * o.x + y * o.y + z * o.z; } }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        // scan whole .data section (menu camera, entities) 0x93a9000-0x12681000
        ScanRange(h, pid, baseAddr, baseAddr + 0x93a9000, baseAddr + 0x12681000, ".data");
        // scan .rdata too
        ScanRange(h, pid, baseAddr, baseAddr + 0x84b4000, baseAddr + 0x93a9000, ".rdata");
        return 0;
    }
}

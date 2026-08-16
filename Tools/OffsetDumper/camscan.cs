using System;
using System.IO;

public class CamScan {
    public static int Main(string[] args) {
        string file = args[0];
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        byte[] d = File.ReadAllBytes(file);

        // Scan .data + .bss region for 4x4 float matrices that look like a view matrix:
        // rotation columns orthonormal (len~1, dot~0), translation = plausible world coords.
        // The cheat reads Matrix4x4 at cameraBase+0x100, so matrix start = (candidate-0x100).
        // We scan for orthonormal 3x3 blocks.
        int hits = 0;
        for (int pos = 0x93a9000; pos + 64 < d.Length && hits < 200; pos += 4) {
            float m00 = BitConverter.ToSingle(d, pos + 0x00);
            float m10 = BitConverter.ToSingle(d, pos + 0x10);
            float m20 = BitConverter.ToSingle(d, pos + 0x20);
            float m01 = BitConverter.ToSingle(d, pos + 0x04);
            float m11 = BitConverter.ToSingle(d, pos + 0x14);
            float m21 = BitConverter.ToSingle(d, pos + 0x24);
            float m02 = BitConverter.ToSingle(d, pos + 0x08);
            float m12 = BitConverter.ToSingle(d, pos + 0x18);
            float m22 = BitConverter.ToSingle(d, pos + 0x28);

            // right = (m00,m10,m20), up = (m01,m11,m21), fwd = (m02,m12,m22)
            float lenR = (float)Math.Sqrt(m00*m00 + m10*m10 + m20*m20);
            float lenU = (float)Math.Sqrt(m01*m01 + m11*m11 + m21*m21);
            float lenF = (float)Math.Sqrt(m02*m02 + m12*m12 + m22*m22);
            if (Math.Abs(lenR - 1) > 0.05) continue;
            if (Math.Abs(lenU - 1) > 0.05) continue;
            if (Math.Abs(lenF - 1) > 0.05) continue;
            float dotRU = m00*m01 + m10*m11 + m20*m21;
            float dotRF = m00*m02 + m10*m12 + m20*m22;
            float dotUF = m01*m02 + m11*m12 + m21*m22;
            if (Math.Abs(dotRU) > 0.15) continue;
            if (Math.Abs(dotRF) > 0.15) continue;
            if (Math.Abs(dotUF) > 0.15) continue;

            float tx = BitConverter.ToSingle(d, pos + 0x30);
            float ty = BitConverter.ToSingle(d, pos + 0x34);
            float tz = BitConverter.ToSingle(d, pos + 0x38);
            // translation should be a plausible coordinate (not huge, not zero-ish garbage)
            if (Math.Abs(tx) > 50000 || Math.Abs(ty) > 50000 || Math.Abs(tz) > 50000) continue;

            // candidate matrix at file offset pos => VA = baseAddr+pos
            Console.WriteLine("MATRIX at fileoff 0x{0:x} (RVA) -> VA 0x{1:x}  rot=(R({2:N3},{3:N3},{4:N3}) U({5:N3},{6:N3},{7:N3}) F({8:N3},{9:N3},{10:N3})) pos=({11:N1},{12:N1},{13:N1})",
                pos, baseAddr + (ulong)pos, m00, m10, m20, m01, m11, m21, m02, m12, m22, tx, ty, tz);
            hits++;
        }
        Console.WriteLine("total matrix hits: " + hits);
        return 0;
    }
}

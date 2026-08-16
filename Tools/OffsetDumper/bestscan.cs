using System;
using System.IO;
using System.Collections.Generic;

public class BestScan {
    public static int Main(string[] args) {
        string file = args[0];
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        byte[] d = File.ReadAllBytes(file);

        // 1) View matrix: finite, orthonormal 3x3, reasonable translation
        Console.WriteLine("=== VIEW MATRICES (finite, orthonormal) ===");
        int mhits = 0;
        for (int pos = 0x93a9000; pos + 64 < d.Length && mhits < 60; pos += 4) {
            float[] m = new float[16];
            for (int i = 0; i < 16; i++) m[i] = BitConverter.ToSingle(d, pos + i * 4);
            bool finite = true;
            foreach (float f in m) if (float.IsNaN(f) || float.IsInfinity(f)) { finite = false; break; }
            if (!finite) continue;
            // rotation 3x3 = m[0..2], m[4..6], m[8..10] (row-major) or column-major - try col-major
            // col-major: col0=(m0,m4,m8) col1=(m1,m5,m9) col2=(m2,m6,m10)
            float c0x=m[0],c0y=m[4],c0z=m[8], c1x=m[1],c1y=m[5],c1z=m[9], c2x=m[2],c2y=m[6],c2z=m[10];
            float l0=(float)Math.Sqrt(c0x*c0x+c0y*c0y+c0z*c0z);
            float l1=(float)Math.Sqrt(c1x*c1x+c1y*c1y+c1z*c1z);
            float l2=(float)Math.Sqrt(c2x*c2x+c2y*c2y+c2z*c2z);
            if (Math.Abs(l0-1)>0.05f||Math.Abs(l1-1)>0.05f||Math.Abs(l2-1)>0.05f) continue;
            float dot01=c0x*c1x+c0y*c1y+c0z*c1z, dot02=c0x*c2x+c0y*c2y+c0z*c2z, dot12=c1x*c2x+c1y*c2y+c1z*c2z;
            if (Math.Abs(dot01)>0.15f||Math.Abs(dot02)>0.15f||Math.Abs(dot12)>0.15f) continue;
            float tx=m[3],ty=m[7],tz=m[11];
            if (Math.Abs(tx)>100000||Math.Abs(ty)>100000||Math.Abs(tz)>100000) continue;
            Console.WriteLine("VM @RVA 0x{0:x} (VA 0x{1:x}) pos=({2:N1},{3:N1},{4:N1}) col0=({5:N2},{6:N2},{7:N2})",
                pos, baseAddr+(ulong)pos, tx,ty,tz, c0x,c0y,c0z);
            mhits++;
        }
        Console.WriteLine("vm hits: " + mhits);

        // 2) Pointer arrays in .data that look like entity lists: run of module-internal pointers
        //    pointing to a common base region, all aligned, with plausible entity structs
        Console.WriteLine("=== POINTER ARRAYS in .data (module-internal, aligned) ===");
        // scan .data section only: VA 0x93a9000, size 0x92d7bcc
        int dataStart = 0x93a9000, dataEnd = 0x93a9000 + 0x92d7bcc;
        for (int pos = dataStart; pos + 8 < dataEnd && pos < dataStart + 0x6000000; pos += 8) {
            ulong v = BitConverter.ToUInt64(d, pos);
            if (v < baseAddr || v >= baseAddr + 0x2591c200) continue;
            ulong rva = v - baseAddr;
            // entity list entries should point into .data heap-ish region or into module
            // count consecutive valid pointers
            int cnt = 0;
            int maxcnt = 0;
            for (int q = pos; q + 8 < dataEnd && q < pos + 0x400; q += 8) {
                ulong vv = BitConverter.ToUInt64(d, q);
                if (vv >= baseAddr && vv < baseAddr + 0x2591c200) cnt++;
                else break;
            }
            if (cnt < 5) continue;
            // candidate: report
            Console.WriteLine("ARRAY @RVA 0x{0:x}: {1} consecutive module ptrs, first->+0x{2:x}", pos, cnt, rva);
            pos += (cnt - 1) * 8;
        }

        // 3) Search for name-array-like: array of pointers to strings in .rdata
        Console.WriteLine("=== STRING POINTER ARRAYS ===");
        int rdataStart = 0x84b4000, rdataEnd = 0x84b4000 + 0xef5000;
        for (int pos = dataStart; pos + 8 < dataEnd && pos < dataStart + 0x4000000; pos += 8) {
            ulong v = BitConverter.ToUInt64(d, pos);
            if (v >= baseAddr + (ulong)rdataStart && v < baseAddr + (ulong)rdataEnd) {
                // check it points to ASCII-ish string
                int soff = (int)(v - baseAddr);
                bool printable = true;
                for (int i = 0; i < 8; i++) {
                    char c = (char)d[soff + i];
                    if (c == 0) break;
                    if (c < 32 || c > 126) { printable = false; break; }
                }
                if (printable) {
                    Console.WriteLine("STRPTR @RVA 0x{0:x}: -> +0x{1:x} '{2}'", pos, soff, System.Text.Encoding.ASCII.GetString(d, soff, Math.Min(16, d.Length - soff)).Split('\0')[0]);
                }
            }
        }
        return 0;
    }
}

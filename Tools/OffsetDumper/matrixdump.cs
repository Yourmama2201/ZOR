using System;
using System.IO;

public class MatrixDump {
    public static int Main(string[] args) {
        string file = args[0];
        byte[] d = File.ReadAllBytes(file);
        // dump 4x4 floats at given RVAs
        string[] rvas = args[1].Split(',');
        foreach (string s in rvas) {
            int rva = Convert.ToInt32(s, 16);
            Console.WriteLine("=== RVA 0x{0:x} ===", rva);
            for (int row = 0; row < 4; row++) {
                string line = "";
                for (int col = 0; col < 4; col++) {
                    int off = rva + row * 16 + col * 4;
                    float f = off + 4 <= d.Length ? BitConverter.ToSingle(d, off) : 0;
                    line += string.Format("{0,12:N4} ", f);
                }
                Console.WriteLine(line);
            }
            // also qword view
            Console.WriteLine("qwords:");
            for (int i = 0; i < 8; i++) {
                int off = rva + i * 8;
                if (off + 8 > d.Length) break;
                Console.WriteLine("  +0x{0:x}: 0x{1:x16}", i * 8, BitConverter.ToUInt64(d, off));
            }
        }
        return 0;
    }
}
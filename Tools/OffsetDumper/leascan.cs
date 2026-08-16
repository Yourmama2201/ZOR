using System;
using System.IO;
using System.Collections.Generic;

public class LeaScan {
    public static int Main(string[] args) {
        // args: <file> <base_hex>
        string file = args[0];
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        byte[] data = File.ReadAllBytes(file);

        var targets = new Dictionary<string, ulong>();
        targets["DISTRIBUTE"] = baseAddr + 0xA88DD10;
        targets["BONE_BASE"] = baseAddr + 0xCB97E48;
        targets["CAMERA_BASE"] = baseAddr + 0x122B1380;
        targets["VIEW_ANGLES"] = baseAddr + 0x12C00;
        targets["LOCAL_POS"] = baseAddr + 0x12C08;
        targets["NAME_ARRAY"] = baseAddr + 0x119C48F8;
        targets["CLIENT_INFO"] = baseAddr + 0x1198C7F8;
        targets["CMD_ARRAY"] = baseAddr + 0xCD3D810;
        targets["GAME_MODE"] = baseAddr + 0xF07D000;
        targets["ACTIVE_STATE"] = baseAddr + 0xF27D528;
        targets["LOOT_PTR"] = baseAddr + 0xB8BD04C;
        targets["WEAPON_DEFS"] = baseAddr + 0x11886930;
        targets["REF_DEF"] = baseAddr + 0x119A2DA0;
        targets["LOCAL_INDEX"] = baseAddr + 0x105150;
        targets["BONE_DISTRIBUTE"] = baseAddr + 0xA88DD10;
        targets["BONE_VISIBLE"] = baseAddr + 0x244B760;
        targets["TIMESTAMP"] = baseAddr + 0x6A18E58F;

        foreach (var kv in targets) {
            ulong tgt = kv.Value;
            int count = 0;
            Console.WriteLine("### {0} target=0x{1:x}", kv.Key, tgt);
            for (int pos = 0; pos + 7 <= data.Length; pos++) {
                // LEA patterns: [REX.W] 8D r/m32/64, [rip+disp32]
                // 48 8D 05|0D|15|1D|25|2D|35|3D <disp32>  ; lea rax/rcx/rdx/.., [rip+disp]
                // 4C 8D 05..                         ; lea r8..r15
                if (data[pos] == 0x48 && data[pos+1] == 0x8D &&
                    (data[pos+2] & 0xC7) == 0x05) {   // mod=00, rm=101 (disp32)
                    int rel = BitConverter.ToInt32(data, pos+3);
                    ulong target = (ulong)(pos + 7) + (ulong)(long)rel;
                    if (target == tgt) {
                        int reg = (data[pos+2] >> 3) & 0x7;
                        Console.WriteLine("  lea r{0},[rip] at fileoff 0x{1:x} disp=0x{2:x}  [{3}]", reg, pos, (uint)rel,
                            GetSection(pos));
                        count++;
                        if (count >= 12) break;
                    }
                }
                if (data[pos] == 0x4C && data[pos+1] == 0x8D &&
                    (data[pos+2] & 0xC7) == 0x05) {
                    int rel = BitConverter.ToInt32(data, pos+3);
                    ulong target = (ulong)(pos + 7) + (ulong)(long)rel;
                    if (target == tgt) {
                        int reg = (data[pos+2] >> 3) & 0x7;
                        Console.WriteLine("  lea r{0},[rip] at fileoff 0x{1:x} disp=0x{2:x}  [{3}]", reg + 8, pos, (uint)rel,
                            GetSection(pos));
                        count++;
                        if (count >= 12) break;
                    }
                }
            }
            if (count == 0) Console.WriteLine("  NO LEA refs found");
        }
        return 0;
    }

    static string GetSection(int off) {
        // second .text starts at 0x12acb000; first .text 0x1000..0x84b2a00
        if (off >= 0x1000 && off < 0x84b2a00) return ".text1";
        if (off >= 0x84b4000 && off < 0x93a9000) return ".rdata";
        if (off >= 0x93a9000 && off < 0x12681000) return ".data";
        if (off >= 0x12acb000 && off < 0x12acb000 + 0x12e51200) return ".text2";
        return "other";
    }
}

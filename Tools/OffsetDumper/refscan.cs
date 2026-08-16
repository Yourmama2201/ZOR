using System;
using System.IO;
using System.Collections.Generic;

public class RefScan {
    public static int Main(string[] args) {
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

        foreach (var kv in targets) {
            ulong tgt = kv.Value;
            int count = 0;
            // Scan for RIP-relative: 48 8B 05/0D/15/1D/25/2D/35/3D disp32 ; also 83/80 xx disp32 for byte ops
            for (int pos = 0; pos + 7 <= data.Length; pos++) {
                // 48 8B 05|0D|15|1D|25|2D|35|3D <disp32>  (mod=00 reg=000/001 disp32)
                if (pos + 6 < data.Length && data[pos] == 0x48 && data[pos+1] == 0x8B &&
                    (data[pos+2] >= 0x05 && data[pos+2] <= 0x3D) && (data[pos+2] & 0x07) == 0x05) {
                    // modrm: mod=00, r/m=101 => disp32
                    int rm = data[pos+2] & 0x07;
                    if (rm == 5) {
                        int rel = BitConverter.ToInt32(data, pos+3);
                        ulong target = (ulong)(pos + 7) + (ulong)(long)rel;
                        if (target == tgt) {
                            Console.WriteLine("{0}: RIP-rel mov at fileoff 0x{1:x} (bytes 48 8B {2:x2} ...) disp=0x{3:x}", kv.Key, pos, data[pos+2], (uint)rel);
                            count++;
                            if (count >= 8) break;
                        }
                    }
                }
                // 48 89 05|0D|15...  (mov [rip+disp], reg) - register is src
                if (pos + 6 < data.Length && data[pos] == 0x48 && data[pos+1] == 0x89 &&
                    (data[pos+2] & 0x07) == 0x05 && ((data[pos+2] >> 3) & 0x07) == 0x05) {
                    int rm = data[pos+2] & 0x07;
                    if (rm == 5) {
                        int rel = BitConverter.ToInt32(data, pos+3);
                        ulong target = (ulong)(pos + 7) + (ulong)(long)rel;
                        if (target == tgt) {
                            Console.WriteLine("{0}: RIP-rel mov-store at fileoff 0x{1:x} (bytes 48 89 {2:x2} ...) disp=0x{3:x}", kv.Key, pos, data[pos+2], (uint)rel);
                            count++;
                            if (count >= 8) break;
                        }
                    }
                }
                // F3 0F 10 05 disp32 (movss xmm0, [rip+disp])
                if (pos + 7 < data.Length && data[pos]==0xF3 && data[pos+1]==0x0F && data[pos+2]==0x10 && data[pos+3]==0x05) {
                    int rel = BitConverter.ToInt32(data, pos+4);
                    ulong target = (ulong)(pos + 8) + (ulong)(long)rel;
                    if (target == tgt) {
                        Console.WriteLine("{0}: movss xmm,[rip] at fileoff 0x{1:x}", kv.Key, pos);
                        count++;
                        if (count >= 8) break;
                    }
                }
                // F3 0F 11 05 disp32 (movss [rip+disp], xmm0)
                if (pos + 7 < data.Length && data[pos]==0xF3 && data[pos+1]==0x0F && data[pos+2]==0x11 && data[pos+3]==0x05) {
                    int rel = BitConverter.ToInt32(data, pos+4);
                    ulong target = (ulong)(pos + 8) + (ulong)(long)rel;
                    if (target == tgt) {
                        Console.WriteLine("{0}: movss [rip],xmm at fileoff 0x{1:x}", kv.Key, pos);
                        count++;
                        if (count >= 8) break;
                    }
                }
            }
            if (count == 0) Console.WriteLine("{0}: no refs found", kv.Key);
        }
        return 0;
    }
}

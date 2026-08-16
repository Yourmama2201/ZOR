using System;
using System.IO;
using System.Collections.Generic;

public class SigScan {
    struct Pattern { public string name; public byte[] bytes; public byte[] mask; public int extra; public bool rel; }

    static Pattern Parse(string name, string hex, int extra, bool rel) {
        var bytes = new List<byte>();
        var mask = new List<byte>();
        var parts = hex.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
        foreach (var p in parts) {
            if (p == "?") { bytes.Add(0); mask.Add(0); }
            else { bytes.Add(Convert.ToByte(p, 16)); mask.Add(1); }
        }
        return new Pattern { name = name, bytes = bytes.ToArray(), mask = mask.ToArray(), extra = extra, rel = rel };
    }

    public static int Main(string[] args) {
        string file = args[0];
        byte[] data = File.ReadAllBytes(file);

        var patterns = new List<Pattern>();
        patterns.Add(Parse("CAMERA_BASE", "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? 48 85 C9", 3, true));
        patterns.Add(Parse("DISTRIBUTE", "48 8B 05 ? ? ? ? 48 8B 48 ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ?", 3, true));
        patterns.Add(Parse("BONE_BASE", "48 8B 05 ? ? ? ? 48 8B 08 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88", 3, true));
        patterns.Add(Parse("VIEW_ANGLES", "F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? 48 83 C4 ? 5B C3", 3, true));
        patterns.Add(Parse("LOCAL_POS", "F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? F3 0F 10 0D ? ? ? ?", 3, true));
        patterns.Add(Parse("CMD_ARRAY", "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ?", 3, true));
        patterns.Add(Parse("WEAPON_DEFS", "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 48 8D 55", 3, true));
        patterns.Add(Parse("ACTIVE_STATE", "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? E8", 3, true));
        patterns.Add(Parse("NAME_ARRAY", "48 8B 05 ? ? ? ? 48 8D 14 80 48 8B 04 D0 48 85 C0 74 ?", 3, true));
        patterns.Add(Parse("GAME_MODE", "83 3D ? ? ? ? ? 75 ? 48 8D 15 ? ? ? ? 48 8D 0D ? ? ? ? E8", 2, true));
        patterns.Add(Parse("LOOT_PTR", "48 8B 0D ? ? ? ? 48 8B 49 ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ?", 3, true));
        patterns.Add(Parse("REF_DEF", "48 8B 05 ? ? ? ? 48 8D 0C 80 48 8B 04 C8 48 85 C0 74 ?", 3, true));
        patterns.Add(Parse("CLIENT_INFO", "48 8B 05 ? ? ? ? 48 33 05 ? ? ? ? 48 8D 0D ? ? ? ? 48 89 05", 3, true));
        patterns.Add(Parse("LOCAL_INDEX", "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 80 ? ? ? ? 48 8B 5C 24 ?", 3, true));
        patterns.Add(Parse("TIMESTAMP", "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 ? 8B 80 ? ? ? ? C3", 3, true));

        foreach (var p in patterns) {
            int count = 0;
            for (int pos = 0; pos + p.bytes.Length <= data.Length; pos++) {
                bool ok = true;
                for (int i = 0; i < p.bytes.Length; i++) {
                    if (p.mask[i] != 0 && data[pos + i] != p.bytes[i]) { ok = false; break; }
                }
                if (ok) {
                    UIntPtr target = new UIntPtr(0);
                    if (p.rel) {
                        int rel = BitConverter.ToInt32(data, pos + p.extra);
                        target = new UIntPtr((ulong)(pos + p.extra + 4 + rel));
                    } else {
                        target = new UIntPtr((ulong)pos);
                    }
                    Console.WriteLine("{0}: match at file-offset 0x{1:x} target=0x{2:x}", p.name, pos, target);
                    count++;
                    if (count > 10) break;
                }
            }
            if (count == 0) Console.WriteLine("{0}: NO MATCH", p.name);
        }
        return 0;
    }
}



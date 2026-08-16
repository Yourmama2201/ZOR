using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

public class ConstScan {
    static void Main(string[] args) {
        string img = @"C:\Users\Admin\Desktop\DMZ_FILES\Tools\OffsetDumper\full_image.bin";
        if (!File.Exists(img)) { Console.Error.WriteLine("NO IMAGE"); return; }
        long fsz = new FileInfo(img).Length;
        Console.WriteLine("Image size: 0x{0:x} ({0} bytes)", fsz);

        ulong[] offs = {
            0xA88DD10, 0xCB97E48, 0x1198C7F8, 0x1908A0, 0xCD3D810, 0xF27D528,
            0x122B1380, 0x12C00, 0x12C08, 0x105150, 0xF07D000, 0x11886930,
            0x119C48F8, 0x119A2DA0, 0xB8BD04C, 0x5000000, 0x118287E8,
            0x90050E6, 0x900510E, 0x90051A8, 0x90051FC, 0xCA37E48,
            0x1236, 0x2F1, 0x520, 0x524, 0x1740, 0x800, 0x1312, 0x17D760,
            0xCD1031C, 0xCD24ADC, 0xC832580, 0xCD1FB30, 0xCD10000, 0xCD26000,
            0x8700, 0x12B0, 0x2F0, 0x5E80, 0xD8, 0xE18, 0x88, 0xDAC, 0xDD4,
            0x13D58, 0x188, 0xD8, 0xA90, 0x244B760, 0x18bc20
        };
        string[] names = {
            "DISTRIBUTE", "BONE_BASE", "CLIENT_INFO", "CLIENT_BASE_OFFSET", "CMD_ARRAY",
            "ACTIVE_STATE", "CAMERA_BASE", "VIEW_ANGLES", "LOCAL_POS", "LOCAL_INDEX",
            "GAME_MODE", "WEAPON_DEFS", "NAME_ARRAY", "REF_DEF", "LOOT_PTR",
            "VEHICLE_LIST", "CLIENT_INFO_ENC(decrypt)", "DECRYPT_A", "DECRYPT_B",
            "DECRYPT_C", "DECRYPT_D", "BONE_ENC(decrypt)", "PLAYER_VALID", "PLAYER_TEAM",
            "PLAYER_HEALTH", "PLAYER_ARMOR", "PLAYER_POS", "PLAYER_NAME", "PLAYER_WEAPON",
            "BONE_BASE_OFFSET", "CAM_MAT_A", "CAM_MAT_B", "CAM_MAT_C", "CAM_MAT_D",
            "CAM_REGION_S", "CAM_REGION_E", "VISIBLE_BIT", "PLAYER_VEL", "LOCAL_INDEX_POS",
            "NAME_ARRAY_POS", "NAME_ARRAY_SIZE", "CENTITY_SIZE", "CENTITY_VALID",
            "CENTITY_ORIGIN", "CENTITY_YAW", "PLAYER_SIZE", "BONE_SIZE", "BONE_OFFSET",
            "BONE_VISIBLE_OFFSET", "BONE_VISIBLE", "CLIENT_BASE_DECRYPT"
        };

        // Build byte-pattern lookup for the *large* distinctive constants only
        var want = new Dictionary<ulong, int>();
        for (int i = 0; i < offs.Length; i++) {
            if (offs[i] >= 0x1000 && offs[i] != 0x12B0 && offs[i] != 0x1236 && offs[i] != 0x2F1 && offs[i] != 0x2F0 && offs[i] != 0x800)
                want[offs[i]] = i;
        }

        var counts = new int[offs.Length];
        var hits = new Dictionary<ulong, List<string>>();

        byte[] buf = new byte[64 * 1024 * 1024];
        using (var fs = new FileStream(img, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)) {
            long pos = 0;
            // overlap window
            byte[] prev = new byte[8];
            while (pos < fsz) {
                int toRead = (int)Math.Min(buf.Length, fsz - pos);
                int got = fs.Read(buf, 0, toRead);
                if (got <= 0) break;

                for (int j = 0; j < got - 3; j++) {
                    if (j % 4 != 0) continue; // align: constants are typically 4-aligned
                    uint v = (uint)(buf[j] | (buf[j + 1] << 8) | (buf[j + 2] << 16) | (buf[j + 3] << 24));
                    if (want.ContainsKey(v)) {
                        long abs = pos + j;
                        counts[want[v]]++;
                        if (!hits.ContainsKey(v)) hits[v] = new List<string>();
                        if (hits[v].Count < 8)
                            hits[v].Add(string.Format("+0x{0:x}", abs));
                    }
                }

                pos += got;
                // carry last 3 bytes for boundary crossing
                // (approximate; acceptable for hit discovery)
            }
        }

        for (int i = 0; i < offs.Length; i++) {
            if (offs[i] < 0x1000 || offs[i] == 0x12B0 || offs[i] == 0x1236 || offs[i] == 0x2F1 || offs[i] == 0x2F0 || offs[i] == 0x800) continue;
            Console.WriteLine("{0} @0x{1:x}: {2} hit(s){3}",
                names[i], offs[i], counts[i],
                hits.ContainsKey(offs[i]) ? "  -> " + string.Join(", ", hits[offs[i]]) : "");
        }
    }
}

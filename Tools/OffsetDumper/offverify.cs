using System;
using System.IO;

public class OffVerify {
    class Off { public string name; public ulong rva; public Off(string n, ulong r) { name=n; rva=r; } }
    public static int Main(string[] args) {
        string file = args[0];
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        byte[] d = File.ReadAllBytes(file);

        Off[] offs = new Off[] {
            new Off("our.CAMERA_BASE",0x122B1380),
            new Off("our.CAMERA_MATRIX",0xCD1031C),
            new Off("our.DISTRIBUTE",0xA88DD10),
            new Off("our.BONE_BASE", 0xCB97E48),
            new Off("our.WEAPON_DEFS",0x11886930),
            new Off("our.CMD_ARRAY", 0xCD3D810),
            new Off("our.ACTIVE_STATE",0xF27D528),
            new Off("our.NAME_ARRAY",0x119C48F8),
            new Off("our.GAME_MODE", 0xF07D000),
            new Off("our.LOOT_PTR",  0xB8BD04C),
            new Off("our.CLIENT_INFO",0x1198C7F8),
            new Off("our.REF_DEF",   0x119A2DA0),
            new Off("wz.RefDef",     0x10768428),
            new Off("wz.LocalPlayer",0x10773280),
            new Off("wz.ENCRYPT_PTR_0x318",0x10764858),
            new Off("wz.REVERSED_0x318",0x37F8275),
            new Off("wz.CG_ENTROPY", 0x3741258),
            new Off("wz.CG_V1",      0x106a10c8),
            new Off("wz.ENCRYPT_PTR_0x3588",0x10765B88),
            new Off("wz.REVERSED_0x3588",0x37F82CA),
            new Off("wz.NAME_ARRAY", 0x107730E8),
            new Off("internal.refdef_ptr",0x130534B0),
            new Off("internal.camera_ptr",0x13672EC0),
            new Off("internal.name_array",0x13072A30),
            new Off("internal.game_mode",0xF8831D8),
            new Off("internal.bones_dist",0xB30F838),
        };

        ulong modEnd = baseAddr + 0x2591c200;
        foreach (Off o in offs) {
            ulong rva = o.rva;
            if (rva + 8 > (ulong)d.Length) { Console.WriteLine("{0,-16} 0x{1:x8}  RVA OUT OF FILE", o.name, rva); continue; }
            ulong v = BitConverter.ToUInt64(d, (int)rva);
            bool inMod = v >= baseAddr && v < modEnd;
            string extra = "";
            if (inMod) extra = "-> mod+0x" + (v - baseAddr).ToString("x8");
            else if (v == 0) extra = " [ZERO/null]";
            Console.WriteLine("{0,-16} RVA 0x{1:x8}  val=0x{2:x16}  {3}", o.name, rva, v, extra);
        }
        return 0;
    }
}
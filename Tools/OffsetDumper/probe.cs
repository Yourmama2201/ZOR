using System;
using System.Runtime.InteropServices;

public class Probe {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFile(string n, uint a, uint s, IntPtr sec, uint d, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool DeviceIoControl(IntPtr h, uint code, byte[] inb, uint ins, byte[] outb, uint outs, out uint ret, IntPtr ov);

    static IntPtr Open() { return CreateFile(@"\\.\ZOR", 0xC0000000u, 0, IntPtr.Zero, 3, 0, IntPtr.Zero); }

    static byte[] Read(IntPtr h, ulong pid, ulong addr, int sz) {
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

    static ulong Ptr(byte[] b, int off) { return BitConverter.ToUInt64(b, off); }
    static float Flt(byte[] b, int off) { return BitConverter.ToSingle(b, off); }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        var b8 = Read(h, pid, baseAddr, 8);
        if (b8 == null) { Console.Error.WriteLine("CANNOT READ"); return 1; }
        Console.WriteLine("MZ at base: 0x{0:x4}", BitConverter.ToUInt16(b8, 0));

        // --- CLIENT INFO DECRYPT CHAIN (mirror of memory.hpp DecryptClientInfo) ---
        Console.WriteLine("=== CLIENT INFO (old build routine) ===");
        ulong clientInfo = 0;
        ulong rdx = Ptr(Read(h, pid, baseAddr + 0x118287E8, 8), 0);
        Console.WriteLine("clientInfoEnc @+0x118287E8 = 0x{0:x}", rdx);
        if (rdx != 0) {
            // peb needed
        }

        // --- Raw reads at offsets ---
        Console.WriteLine("=== RAW GLOBAL READS ===");
        ulong[] addrs = {
            0x12C00, 0x12C08, 0x105150, 0xA88DD10, 0xCB97E48, 0xB8BD04C,
            0x119C48F8, 0x1198C7F8, 0x119A2DA0, 0xF07D000, 0xF27D528,
            0xCD3D810, 0x122B1380, 0x11886930, 0x5000000, 0x1908A0
        };
        string[] names = {
            "VIEW_ANGLES", "LOCAL_POS", "LOCAL_INDEX", "DISTRIBUTE", "BONE_BASE",
            "LOOT_PTR", "NAME_ARRAY", "CLIENT_INFO", "REF_DEF", "GAME_MODE",
            "ACTIVE_STATE", "CMD_ARRAY", "CAMERA_BASE", "WEAPON_DEFS",
            "VEHICLE_LIST", "CLIENT_BASE"
        };
        for (int i = 0; i < addrs.Length; i++) {
            var b = Read(h, pid, baseAddr + addrs[i], 8);
            if (b == null) { Console.WriteLine("{0}: READ FAIL", names[i]); continue; }
            ulong v = Ptr(b, 0);
            float f0 = Flt(b, 0), f1 = Flt(b, 4);
            string tag = "";
            // is it a module-internal pointer?
            if (v >= baseAddr && v < baseAddr + 0x2591c200) tag = " [IN-MODULE +0x" + (v - baseAddr).ToString("x") + "]";
            else if (v == 0) tag = " [null]";
            else if (v == 0xffffffffffffffff) tag = " [0xff..]";
            Console.WriteLine("{0} @+0x{1:x}: 0x{2:x}  f0={3} f1={4}{5}", names[i], addrs[i], v, f0, f1, tag);
        }

        // --- DISTRIBUTE chain ---
        Console.WriteLine("=== DISTRIBUTE CHAIN ===");
        ulong dist = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8), 0);
        Console.WriteLine("dist list = 0x{0:x} (module+0x{1:x})", dist, dist - baseAddr);
        for (int i = 0; i < 12; i++) {
            var b = Read(h, pid, dist + (ulong)(i * 8), 8);
            if (b == null) { Console.WriteLine("  [{0}] READ FAIL", i); continue; }
            ulong e = Ptr(b, 0);
            if (e == 0) continue;
            var eb = Read(h, pid, e, 8);
            if (eb == null) { Console.WriteLine("  [{0}] ent=0x{1:x} (READ FAIL)", i, e); continue; }
            ulong e0 = Ptr(eb, 0);
            string tag = (e >= baseAddr && e < baseAddr + 0x2591c200) ? " [IN-MODULE]" : "";
            Console.WriteLine("  [{0}] ent=0x{1:x} first=0x{2:x}{3}", i, e, e0, tag);
        }

        // --- BONE_BASE chain ---
        Console.WriteLine("=== BONE BASE ===");
        ulong bone = Ptr(Read(h, pid, baseAddr + 0xCB97E48, 8), 0);
        Console.WriteLine("bone base (raw) = 0x{0:x} (module+0x{1:x})", bone, bone - baseAddr);

        // --- Check decryption helper globals ---
        Console.WriteLine("=== DECRYPTION HELPERS (old build addresses) ===");
        ulong[] helper = { 0x90050E6, 0x900510E, 0x90051A8, 0x90051FC, 0x9005000 };
        foreach (ulong off in helper) {
            var b = Read(h, pid, baseAddr + off, 8);
            if (b == null) { Console.WriteLine("  +0x{0:x}: READ FAIL", off); continue; }
            ulong v = Ptr(b, 0);
            Console.WriteLine("  +0x{0:x}: 0x{1:x}", off, v);
        }
        return 0;
    }
}

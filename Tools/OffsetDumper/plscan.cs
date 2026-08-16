using System;
using System.Runtime.InteropServices;

public class PlScan {
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

    // For a candidate entity base in .data, check old field offsets look like a player
    static void Check(IntPtr h, uint pid, ulong baseAddr, ulong addr, int count, string label) {
        var b = Read(h, pid, addr, 0x2000);
        if (b == null) return;
        int hits = 0;
        for (int i = 0; i + 0x1800 < b.Length; i += 8) {
            // +0x520 health float, +0x2F1 team byte, +0x1740 pos
            float hp = BitConverter.ToSingle(b, i + 0x520);
            float armor = BitConverter.ToSingle(b, i + 0x524);
            float px = BitConverter.ToSingle(b, i + 0x1740);
            float py = BitConverter.ToSingle(b, i + 0x1744);
            float pz = BitConverter.ToSingle(b, i + 0x1748);
            byte team = b[i + 0x2F1];
            byte valid = b[i + 0x1236];
            bool hpOk = (hp >= 0 && hp <= 300) || hp == -1;
            bool posOk = Finite(px) && Finite(py) && Finite(pz) && Math.Abs(px) < 50000 && Math.Abs(py) < 50000 && Math.Abs(pz) < 50000;
            if (hpOk && posOk && team < 8) {
                ulong a = addr + (ulong)i;
                Console.WriteLine("{0} P? @ mod+0x{1:x} hp={2:0.#} ar={3:0.#} team={4} valid={5} pos=({6:0.#},{7:0.#},{8:0.#})", label, a - baseAddr, hp, armor, team, valid, px, py, pz);
                if (++hits >= 15) break;
            }
        }
    }

    public static int Main(string[] args) {
        uint pid = uint.Parse(args[0]);
        ulong baseAddr = ulong.Parse(args[1], System.Globalization.NumberStyles.HexNumber);
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }
        // scan .data in windows
        ulong start = baseAddr + 0x93a9000, end = baseAddr + 0x12681000;
        int win = 0x40000;
        int tot = 0;
        for (ulong off = 0; off < end - start; off += (ulong)win) {
            ulong s = start + off;
            var b = Read(h, pid, s, win);
            if (b == null) continue;
            for (int i = 0; i + 0x1800 < b.Length; i += 8) {
                float hp = BitConverter.ToSingle(b, i + 0x520);
                float armor = BitConverter.ToSingle(b, i + 0x524);
                float px = BitConverter.ToSingle(b, i + 0x1740);
                float py = BitConverter.ToSingle(b, i + 0x1744);
                float pz = BitConverter.ToSingle(b, i + 0x1748);
                byte team = b[i + 0x2F1];
                if (!(hp >= 0 && hp <= 300)) continue;
                if (!(Finite(px) && Finite(py) && Finite(pz) && Math.Abs(px) < 50000 && Math.Abs(py) < 50000 && Math.Abs(pz) < 50000)) continue;
                if (team > 8) continue;
                Console.WriteLine("P? @ mod+0x{0:x} hp={1:0.#} ar={2:0.#} team={3} pos=({4:0.#},{5:0.#},{6:0.#})", (s - baseAddr) + (ulong)i, hp, armor, team, px, py, pz);
                if (++tot >= 30) return 0;
            }
        }
        if (tot == 0) Console.WriteLine("no player-struct matches in .data");
        return 0;
    }
}

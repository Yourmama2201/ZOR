using System;
using System.IO;
using System.Runtime.InteropServices;

public class InjectTest {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFile(string n, uint a, uint s, IntPtr sec, uint d, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool DeviceIoControl(IntPtr h, uint code, byte[] inb, uint ins, byte[] outb, uint outs, out uint ret, IntPtr ov);
    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr h);

    static IntPtr Open() { return CreateFile(@"\\.\ZOR", 0xC0000000u, 0, IntPtr.Zero, 3, 0, IntPtr.Zero); }

    static uint Ctl(int func) { return (uint)((0x22 << 16) | (0 << 14) | (func << 2) | 0); }
    static uint IOCTL_ALLOC = Ctl(0x900), IOCTL_EXEC = Ctl(0x901), IOCTL_READ = Ctl(0x800), IOCTL_WRITE = Ctl(0x801);

    [StructLayout(LayoutKind.Sequential)]
    struct ALLOC_REQ { public ulong ProcessId; public ulong SizeOfImage; public ulong RemoteBase; public uint ErrorStatus; }
    [StructLayout(LayoutKind.Sequential)]
    struct EXEC_REQ { public ulong ProcessId; public ulong RemoteBase; public uint ErrorStatus; }
    [StructLayout(LayoutKind.Sequential)]
    struct MEM_REQ { public ulong ProcessId; public ulong Address; public ulong Buffer; public ulong Size; }

    static byte[] ReadMem(IntPtr h, ulong pid, ulong addr, int sz) {
        var buf = new byte[32 + sz];
        BitConverter.GetBytes(pid).CopyTo(buf, 0);
        BitConverter.GetBytes(addr).CopyTo(buf, 8);
        BitConverter.GetBytes(0UL).CopyTo(buf, 16);
        BitConverter.GetBytes((long)sz).CopyTo(buf, 24);
        uint ret = 0;
        bool ok = DeviceIoControl(h, IOCTL_READ, buf, (uint)buf.Length, buf, (uint)buf.Length, out ret, IntPtr.Zero);
        if (ok && ret >= 32) { var r = new byte[sz]; Array.Copy(buf, 32, r, 0, sz); return r; }
        return null;
    }

    public static int Main() {
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }
        Console.WriteLine("device open ok");

        // find game pid
        uint pid = 0;
        foreach (var p in System.Diagnostics.Process.GetProcesses()) {
            try { if (p.ProcessName.Equals("cod22-cod", StringComparison.OrdinalIgnoreCase)) { pid = (uint)p.Id; break; } } catch { }
        }
        if (pid == 0) { Console.WriteLine("game not found"); return 1; }
        Console.WriteLine("game PID = " + pid);

        byte[] dll = File.ReadAllBytes(@"C:\Users\Admin\Desktop\DMZ_FILES\Client\x64\Release\ZORClient.dll");
        Console.WriteLine("DLL " + dll.Length + " bytes");

        // Phase 1: ALLOC
        var alloc = new ALLOC_REQ();
        alloc.ProcessId = pid;
        alloc.SizeOfImage = (ulong)dll.Length;
        byte[] ab = new byte[Marshal.SizeOf(alloc)];
        Marshal.StructureToPtr(alloc, Marshal.AllocHGlobal(ab.Length), false);
        var abuf = new byte[32];
        byte[] raw = new byte[32];
        BitConverter.GetBytes(pid).CopyTo(raw, 0);
        BitConverter.GetBytes((long)dll.Length).CopyTo(raw, 8);
        uint r1 = 0;
        bool ok1 = DeviceIoControl(h, IOCTL_ALLOC, raw, 32, raw, 32, out r1, IntPtr.Zero);
        ulong remoteBase = BitConverter.ToUInt64(raw, 16);
        uint err1 = BitConverter.ToUInt32(raw, 24);
        Console.WriteLine("ALLOC ok=" + ok1 + " bytes=" + r1 + " remoteBase=0x" + remoteBase.ToString("x") + " err=0x" + err1.ToString("X8"));
        if (!ok1 || remoteBase == 0) { Console.WriteLine("ALLOC FAILED"); return 1; }

        // Phase 2: WRITE image in chunks
        const int chunk = 0x4000;
        for (int off = 0; off < dll.Length; off += chunk) {
            int n = Math.Min(chunk, dll.Length - off);
            var wr = new byte[32 + n];
            BitConverter.GetBytes(pid).CopyTo(wr, 0);
            BitConverter.GetBytes(remoteBase + (ulong)off).CopyTo(wr, 8);
            BitConverter.GetBytes((long)n).CopyTo(wr, 24);
            Array.Copy(dll, off, wr, 32, n);
            uint rr = 0;
            bool wok = DeviceIoControl(h, IOCTL_WRITE, wr, (uint)wr.Length, wr, (uint)wr.Length, out rr, IntPtr.Zero);
            if (!wok) { Console.WriteLine("WRITE fail at off 0x" + off.ToString("x") + " err=" + Marshal.GetLastWin32Error()); return 1; }
        }
        Console.WriteLine("WRITE all chunks done (" + dll.Length + " bytes)");

        // Phase 3: EXEC
        var er = new byte[24];
        BitConverter.GetBytes(pid).CopyTo(er, 0);
        BitConverter.GetBytes(remoteBase).CopyTo(er, 8);
        uint r2 = 0;
        bool ok2 = DeviceIoControl(h, IOCTL_EXEC, er, 24, er, 24, out r2, IntPtr.Zero);
        uint execErr = BitConverter.ToUInt32(er, 16);
        Console.WriteLine("EXEC ok=" + ok2 + " bytes=" + r2 + " err=0x" + execErr.ToString("X8"));
        if (ok2 && execErr == 0) Console.WriteLine("SUCCESS - INJECTED");
        else Console.WriteLine("EXEC FAILED err=0x" + execErr.ToString("X8"));

        // Verify: read first bytes of remote base
        var hdr = ReadMem(h, pid, remoteBase, 64);
        if (hdr != null) Console.WriteLine("remote image MZ check: 0x" + hdr[0].ToString("x2") + hdr[1].ToString("x2"));
        CloseHandle(h);
        return 0;
    }
}

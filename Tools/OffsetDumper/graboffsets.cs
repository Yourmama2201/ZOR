using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

public class GrabOffsets {
    // ---- driver comms ----
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFile(string n, uint a, uint s, IntPtr sec, uint d, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool DeviceIoControl(IntPtr h, uint code, byte[] inb, uint ins, byte[] outb, uint outs, out uint ret, IntPtr ov);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool CreateProcess(string app, string cmd, IntPtr pa, IntPtr ta, bool inh, uint flags, IntPtr env, string dir, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr h);

    [StructLayout(LayoutKind.Sequential)]
    struct STARTUPINFO { public int cb; public IntPtr lpReserved, lpDesktop, lpTitle; public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags; public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError; }
    [StructLayout(LayoutKind.Sequential)]
    struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public uint dwProcessId, dwThreadId; }

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
    static int Int32(byte[] b, int off) { return BitConverter.ToInt32(b, off); }

    static ulong baseAddr;
    static uint pid;
    static readonly ulong IMG_SIZE = 0x2591c200;

    static string Classify(ulong v) {
        if (v == 0) return "[null]";
        if (v == 0xffffffffffffffff) return "[0xff sentinel]";
        if (v >= baseAddr && v < baseAddr + IMG_SIZE) {
            ulong off = v - baseAddr;
            string sec = SectionOf(off);
            return "[IN-MODULE +0x" + off.ToString("x") + " " + sec + "]";
        }
        return "[0x" + v.ToString("x") + "]";
    }
    static string SectionOf(ulong off) {
        if (off < 0x84b2a00) return ".text1";
        if (off < 0x93a9000) return ".rdata";
        if (off < 0x12681000) return ".data";
        if (off < 0x12acb000) return ".pdata";
        if (off < 0x2591c200) return ".text2";
        return "?";
    }

    static void Global(IntPtr h, string name, ulong off, out ulong val) {
        var b = Read(h, pid, baseAddr + off, 8);
        if (b == null) { Console.WriteLine("  {0,-18} +0x{1:x8}  READ FAIL", name, off); val = 0; return; }
        val = Ptr(b, 0);
        float f0 = Flt(b, 0), f1 = Flt(b, 4);
        Console.WriteLine("  {0,-18} +0x{1:x8} = 0x{2:x16}  f0={3,-10:0.00} f1={4,-10:0.00} {5}",
            name, off, val, f0, f1, Classify(val));
    }

    public static int Main(string[] args) {
        // Launch game if not running
        const string gameExe = "C:\\Program Files (x86)\\Call of Duty Modern Warfare II\\_retail_\\cod22-cod.exe";
        IntPtr h = Open();
        if (h.ToInt64() == -1) { Console.Error.WriteLine("OPEN FAILED " + Marshal.GetLastWin32Error()); return 1; }

        // find pid
        pid = FindPid("cod22-cod.exe");
        if (pid == 0) {
            Console.WriteLine("Game not running. Launching: " + gameExe);
            STARTUPINFO si = new STARTUPINFO(); si.cb = Marshal.SizeOf(si);
            PROCESS_INFORMATION pi;
            if (CreateProcess(null, gameExe, IntPtr.Zero, IntPtr.Zero, false, 0x00000004 /*CREATE_NO_WINDOW removed - use 0*/ | 0, IntPtr.Zero, null, ref si, out pi)) {
                Console.WriteLine("Launched PID=" + pi.dwProcessId);
            } else {
                Console.WriteLine("Launch failed err=" + Marshal.GetLastWin32Error() + ", waiting for existing process...");
            }
            for (int i = 0; i < 120 && pid == 0; i++) { Thread.Sleep(1000); pid = FindPid("cod22-cod.exe"); if (pid == 0) Console.Write("."); }
            Console.WriteLine();
        }
        if (pid == 0) { Console.Error.WriteLine("Game PID not found."); return 1; }
        Console.WriteLine("PID = " + pid);

        // get base via user-mode PEB (NtQueryInformationProcess works even with AC),
        // then read PEB+0x10 through the driver Read IOCTL.
        ulong peb = GetPeb(pid);
        Console.WriteLine("PEB = 0x" + peb.ToString("x"));
        if (peb != 0) {
            var bb = Read(h, pid, peb + 0x10, 8);
            if (bb != null) { baseAddr = Ptr(bb, 0); Console.WriteLine("base via PEB+0x10 = 0x" + baseAddr.ToString("x")); }
        }
        if (baseAddr == 0) {
            // Fallback: first module via toolhelp
            baseAddr = FirstModuleBase(pid);
            Console.WriteLine("toolhelp base=0x" + baseAddr.ToString("x"));
        }
        if (baseAddr == 0) {
            Console.Error.WriteLine("Could not determine base.");
            return 1;
        }
        Console.WriteLine("Base = 0x" + baseAddr.ToString("x"));

        Console.WriteLine("\n=========== GLOBAL SLOTS ===========");
        ulong clientInfo, distribute, boneBase, cameraBase, localIndex, viewAngles, localPos, cmdArray, activeState, gameMode, weaponDefs, nameArray, refDef, lootPtr, vehicleList;
        Global(h, "CLIENT_INFO", 0x1198C7F8, out clientInfo);
        Global(h, "DISTRIBUTE", 0xA88DD10, out distribute);
        Global(h, "BONE_BASE", 0xCB97E48, out boneBase);
        Global(h, "CAMERA_BASE", 0x122B1380, out cameraBase);
        Global(h, "LOCAL_INDEX", 0x105150, out localIndex);
        Global(h, "VIEW_ANGLES", 0x12C00, out viewAngles);
        Global(h, "LOCAL_POS", 0x12C08, out localPos);
        Global(h, "CMD_ARRAY", 0xCD3D810, out cmdArray);
        Global(h, "ACTIVE_STATE", 0xF27D528, out activeState);
        Global(h, "GAME_MODE", 0xF07D000, out gameMode);
        Global(h, "WEAPON_DEFS", 0x11886930, out weaponDefs);
        Global(h, "NAME_ARRAY", 0x119C48F8, out nameArray);
        Global(h, "REF_DEF", 0x119A2DA0, out refDef);
        Global(h, "LOOT_PTR", 0xB8BD04C, out lootPtr);
        Global(h, "VEHICLE_LIST", 0x5000000, out vehicleList);
        ulong dump1, dump2;
        Global(h, "CAMERA_ALT", 0xcd24adc, out dump1); // dup seen earlier
        Global(h, "CAMERA_MAT", 0xcd1031c, out dump2); // known menu camera matrix

        Console.WriteLine("\n=========== DISTRIBUTE TABLE WALK ===========");
        if (distribute >= baseAddr && distribute < baseAddr + IMG_SIZE) {
            for (int i = 0; i < 20; i++) {
                var b = Read(h, pid, distribute + (ulong)(i * 16), 16);
                if (b == null) continue;
                ulong p0 = Ptr(b, 0), p1 = Ptr(b, 8);
                Console.WriteLine("  [{0,2}] p0={1} p1={2}", i, Classify(p0), Classify(p1));
            }
            // Does client's 8-byte-stride work? dump 8-byte stride reads at same base
            Console.WriteLine("  -- 8-byte stride (client main.cpp style) --");
            for (int i = 0; i < 10; i++) {
                var b = Read(h, pid, distribute + (ulong)(i * 8), 8);
                if (b == null) continue;
                ulong p0 = Ptr(b, 0);
                Console.WriteLine("  [{0,2}] {1}", i, Classify(p0));
            }
        } else {
            Console.WriteLine("  DISTRIBUTE not a module pointer (0x" + distribute.ToString("x") + ")");
        }

        Console.WriteLine("\n=========== CLIENT INFO CHAIN ===========");
        if (clientInfo >= baseAddr && clientInfo < baseAddr + IMG_SIZE) {
            ulong ci = clientInfo;
            for (int depth = 0; depth < 8; depth++) {
                var b = Read(h, pid, ci, 8);
                if (b == null) { Console.WriteLine("  depth {0}: READ FAIL", depth); break; }
                ulong v = Ptr(b, 0);
                Console.WriteLine("  depth {0}: 0x{1:x} -> {2}", depth, ci - baseAddr, Classify(v));
                if (v == 0 || v == 0xffffffffffffffff) break;
                if (v < baseAddr || v >= baseAddr + IMG_SIZE) { Console.WriteLine("  (not in module - stopping)"); break; }
                ci = v;
            }
        } else {
            Console.WriteLine("  CLIENT_INFO sentinel or not module ptr (in menu, expected 0xff)");
        }

        Console.WriteLine("\n=========== CAMERA MATRIX (menu) ===========");
        for (int k = 0; k < 2; k++) {
            ulong addr = baseAddr + (ulong)(k == 0 ? 0xcd1031c : 0xcd24adc);
            var b = Read(h, pid, addr, 64);
            if (b == null) { Console.WriteLine("  +0x{0:x} READ FAIL", addr - baseAddr); continue; }
            Console.Write("  +0x{0:x}:", addr - baseAddr);
            for (int i = 0; i < 16; i += 4) Console.Write(" {0:0.###},{1:0.###},{2:0.###},{3:0.###}", Flt(b, i), Flt(b, i + 4), Flt(b, i + 8), Flt(b, i + 12));
            Console.WriteLine();
        }

        Console.WriteLine("\n=========== BONE BASE CHAIN ===========");
        if (boneBase >= baseAddr && boneBase < baseAddr + IMG_SIZE) {
            for (int i = 0; i < 4; i++) {
                var b = Read(h, pid, boneBase + (ulong)(i * 8), 8);
                if (b == null) break;
                ulong v = Ptr(b, 0);
                Console.WriteLine("  [{0}] {1}", i, Classify(v));
            }
        }

        Console.WriteLine("\n=========== RAW STRING/STRUCT PROBES ===========");
        // name array probe
        if (nameArray != 0 && nameArray < baseAddr + IMG_SIZE && nameArray > baseAddr) {
            Console.WriteLine("  NAME_ARRAY@+0x{0:x} first qword: {1}", nameArray - baseAddr, Classify(Ptr(Read(h, pid, nameArray, 8) ?? new byte[8], 0)));
        }
        if (localIndex != 0 && localIndex < baseAddr + IMG_SIZE && localIndex > baseAddr) {
            var lb = Read(h, pid, localIndex, 8);
            Console.WriteLine("  LOCAL_INDEX@+0x{0:x} = {1}", localIndex - baseAddr, Classify(Ptr(lb ?? new byte[8], 0)));
        }
        if (lootPtr != 0 && lootPtr >= baseAddr && lootPtr < baseAddr + IMG_SIZE) {
            Console.WriteLine("  LOOT_PTR@+0x{0:x} first qword: {1}", lootPtr - baseAddr, Classify(Ptr(Read(h, pid, lootPtr, 8) ?? new byte[8], 0)));
        }
        if (refDef != 0 && refDef >= baseAddr && refDef < baseAddr + IMG_SIZE) {
            Console.WriteLine("  REF_DEF@+0x{0:x} first qword: {1}", refDef - baseAddr, Classify(Ptr(Read(h, pid, refDef, 8) ?? new byte[8], 0)));
        }

        Console.WriteLine("\nDone. Base=0x{0:x} PID={1}", baseAddr, pid);

        // Optional report-function scan (needs in-match):
        //   --reportscan            scan report strings + xrefs
        //   --sig "AA BB ?? CC"     raw byte pattern scan over .text
        //   --weaponscan            dump WEAPON_DEFS structure + gun names
        //   --bones <playerIdx>     dump all bone positions for a distribute player
        // All-in-one mode: no args => run every scan automatically.
        bool allInOne = args.Length == 0;
        var runArgs = new List<string>();
        if (allInOne) {
            runArgs.Add("--offsets");
            runArgs.Add("--weaponscan");
            runArgs.Add("--reportscan");
            runArgs.Add("--visible");
            runArgs.Add("--bones");
            runArgs.Add("0");
            runArgs.Add("--weaponidx");
            runArgs.Add("0");
            runArgs.Add("--deaths");
        }
        else foreach (var a in args) runArgs.Add(a);
        if (allInOne) Console.WriteLine("ALL-IN-ONE MODE (no args) - running every scan");

        for (int ai = 0; ai < runArgs.Count; ai++) {
            string cur = runArgs[ai];
            if (cur == "--offsets") {
                Console.WriteLine("\n=========== ALL GLOBAL OFFSETS ===========");
                ulong[][] pairs = {
                    new ulong[]{ 0x1198C7F8 }, new ulong[]{ 0xA88DD10 }, new ulong[]{ 0xCB97E48 },
                    new ulong[]{ 0x122B1380 }, new ulong[]{ 0x105150 }, new ulong[]{ 0x12C00 },
                    new ulong[]{ 0x12C08 }, new ulong[]{ 0xCD3D810 }, new ulong[]{ 0xF27D528 },
                    new ulong[]{ 0xF07D000 }, new ulong[]{ 0x11886930 }, new ulong[]{ 0x119C48F8 },
                    new ulong[]{ 0x119A2DA0 }, new ulong[]{ 0xB8BD04C }, new ulong[]{ 0x5000000 },
                    new ulong[]{ 0xCD1031C }, new ulong[]{ 0xCD24ADC }
                };
                string[] names = {
                    "CLIENT_INFO","DISTRIBUTE","BONE_BASE","CAMERA_BASE","LOCAL_INDEX","VIEW_ANGLES",
                    "LOCAL_POS","CMD_ARRAY","ACTIVE_STATE","GAME_MODE","WEAPON_DEFS","NAME_ARRAY",
                    "REF_DEF","LOOT_PTR","VEHICLE_LIST","CAMERA_MATRIX","CAMERA_ALT"
                };
                for (int k = 0; k < names.Length; k++) {
                    ulong v;
                    Global(h, names[k], pairs[k][0], out v);
                }
            }
            if (cur == "--reportscan") {
                Console.WriteLine("\n=========== REPORT FUNCTION SCAN ===========");
                string[] needles = {
                    "report", "ReportPlayer", "report_player", "REPORT_PLAYER",
                    "player_report", "PlayerReport", "reportPlayer", "OnReportPlayer",
                    "SubmitReport", "submit_report", "g_report", "report_uid"
                };
                var found = new List<string>();
                foreach (var nd in needles) {
                    ulong a = FindAsciiString(h, nd);
                    if (a != 0) {
                        Console.WriteLine("  string \"" + nd + "\" @ +0x" + (a - baseAddr).ToString("x"));
                        ScanXRefs(h, a, nd);
                        found.Add(nd);
                    }
                }
                if (found.Count == 0) Console.WriteLine("  (no report strings found in .rdata)");
            }
            if (cur == "--sig" && ai + 1 < runArgs.Count) {
                Console.WriteLine("\n=========== PATTERN SCAN ===========");
                ScanForPattern(h, runArgs[ai + 1]);
            }
            if (cur == "--weaponscan") {
                Console.WriteLine("\n=========== WEAPON DEFS SCAN ===========");
                ulong wd = Ptr(Read(h, pid, baseAddr + 0x11886930, 8) ?? new byte[8], 0);
                if (wd < baseAddr || wd >= baseAddr + IMG_SIZE) {
                    Console.WriteLine("  WEAPON_DEFS not module ptr: 0x" + wd.ToString("x"));
                } else {
                    Console.WriteLine("  WEAPON_DEFS @ +0x{0:x}", wd - baseAddr);
                    var cntb = Read(h, pid, wd + 0x8, 4);
                    int count = cntb != null ? BitConverter.ToInt32(cntb, 0) : 0;
                    Console.WriteLine("  count@+0x8 = " + count);
                    if (count > 600) count = 600;
                    int shown = 0;
                    for (int i = 0; i < count && shown < 40; i++) {
                        var eb = Read(h, pid, wd + 0x20 + (ulong)(i * 0x8), 8);
                        if (eb == null) continue;
                        ulong entry = Ptr(eb, 0);
                        if (entry == 0) continue;
                        var nb = Read(h, pid, entry + 0x10, 48);
                        if (nb == null) continue;
                        string nm = System.Text.Encoding.ASCII.GetString(nb);
                        int nz = nm.IndexOf('\0'); if (nz >= 0) nm = nm.Substring(0, nz);
                        Console.WriteLine("  [{0,3}] entry=+0x{1:x} name=\"{2}\"", i, entry - baseAddr, nm);
                        shown++;
                    }
                    if (shown == 0) Console.WriteLine("  (no weapon names readable - may need in-match)");
                }
            }
            if (cur == "--bones" && ai + 1 < runArgs.Count) {
                Console.WriteLine("\n=========== PLAYER BONE DUMP ===========");
                int idx;
                if (int.TryParse(runArgs[ai + 1], out idx)) {
                    ulong el = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8) ?? new byte[8], 0);
                    if (el != 0) {
                        var eb = Read(h, pid, el + (ulong)(idx * 0x8), 8);
                        if (eb != null) {
                            ulong e = Ptr(eb, 0);
                            if (e != 0) {
                                Console.WriteLine("  player[{0}] @ +0x{1:x}", idx, e - baseAddr);
                                var bb = Read(h, pid, e + 0x17D760, 8);
                                ulong bBoneBase = bb != null ? Ptr(bb, 0) : 0;
                                Console.WriteLine("  boneBase @ +0x{0:x} = 0x{1:x}", 0x17D760, bBoneBase);
                                if (bBoneBase != 0) {
                                    for (int b = 0; b < 24; b++) {
                                        var bp = Read(h, pid, bBoneBase + (ulong)(b * 0x8), 8);
                                        if (bp == null) continue;
                                        ulong bonePtr = Ptr(bp, 0);
                                        if (bonePtr == 0) continue;
                                        var pos = Read(h, pid, bonePtr + (ulong)(b * 0x30) + 0x10, 12);
                                        if (pos == null) continue;
                                        Console.WriteLine("    bone[{0,2}] ptr=+0x{1:x} pos=({2:0.0},{3:0.0},{4:0.0})",
                                            b, bonePtr - baseAddr, Flt(pos, 0), Flt(pos, 4), Flt(pos, 8));
                                    }
                                } else {
                                    Console.WriteLine("  (boneBase null - try --bones with a valid distribute slot)");
                                }
                            }
                        }
                    }
                }
            }
            if (cur == "--weaponidx" && ai + 1 < runArgs.Count) {
                // Dump the current-weapon index + resolved WEAPON_DEFS name for a
                // distribute slot, at every relevant player struct offset.
                Console.WriteLine("\n=========== PLAYER WEAPON IDX ===========");
                int idx;
                if (int.TryParse(runArgs[ai + 1], out idx)) {
                    ulong el = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8) ?? new byte[8], 0);
                    if (el != 0) {
                        var eb = Read(h, pid, el + (ulong)(idx * 0x8), 8);
                        if (eb != null) {
                            ulong e = Ptr(eb, 0);
                            if (e != 0) {
                                Console.WriteLine("  player[{0}] @ +0x{1:x}", idx, e - baseAddr);
                                ulong wd = Ptr(Read(h, pid, baseAddr + 0x11886930, 8) ?? new byte[8], 0);
                                int[] offsets = { 0x10, 0x14, 0x18, 0x1312, 0x1314, 0x120, 0x124 };
                                foreach (int o in offsets) {
                                    var ib = Read(h, pid, e + (ulong)o, 4);
                                    if (ib == null) continue;
                                    int w = BitConverter.ToInt32(ib, 0);
                                    string nm = "?";
                                    if (w > 0 && w < 600 && wd != 0) {
                                        var en = Read(h, pid, wd + 0x20 + (ulong)(w * 0x8), 8);
                                        if (en != null) {
                                            ulong entry = Ptr(en, 0);
                                            var nb = Read(h, pid, entry + 0x10, 48);
                                            if (nb != null) {
                                                nm = System.Text.Encoding.ASCII.GetString(nb);
                                                int nz = nm.IndexOf('\0'); if (nz >= 0) nm = nm.Substring(0, nz);
                                            }
                                        }
                                    }
                                    Console.WriteLine("    +0x{0:x4} = {1}  -> \"{2}\"", o, w, nm);
                                }
                            }
                        }
                    }
                }
            }
            if (cur == "--visible") {
                // Scan candidate player-struct offsets for the visibility bit: look
                // for bytes that read 1 on a currently-visible distribute slot.
                Console.WriteLine("\n=========== VISIBILITY BIT PROBE ===========");
                ulong el = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8) ?? new byte[8], 0);
                if (el == 0) { Console.WriteLine("  DISTRIBUTE null (needs in-match)"); }
                else {
                    for (int idx = 0; idx < 12; idx++) {
                        var eb = Read(h, pid, el + (ulong)(idx * 0x8), 8);
                        if (eb == null) continue;
                        ulong e = Ptr(eb, 0);
                        if (e == 0) continue;
                        int[] offs = { 0x8700, 0x8698, 0x8688, 0x8704, 0x8740, 0x8750 };
                        foreach (int o in offs) {
                            var vb = Read(h, pid, e + (ulong)o, 1);
                            if (vb == null) continue;
                            byte v = vb[0];
                            if (v == 1 || v == 2 || v == 3) Console.WriteLine("    player[{0}] +0x{1:x} = {2}  (candidate vis byte)", idx, o, v);
                        }
                    }
                }
            }
            if (cur == "--deaths") {
                // Probe for a kill/death counter on the local player (scoreboard feed).
                Console.WriteLine("\n=========== KILL/DEATH COUNTER PROBE ===========");
                // local index
                var lib = Read(h, pid, baseAddr + 0x105150, 4);
                int li = lib != null ? BitConverter.ToInt32(lib, 0) : 0;
                Console.WriteLine("  LOCAL_INDEX = " + li);
                if (li > 0 && li < 200) {
                    ulong el = Ptr(Read(h, pid, baseAddr + 0xA88DD10, 8) ?? new byte[8], 0);
                    if (el != 0) {
                        var eb = Read(h, pid, el + (ulong)(li * 0x8), 8);
                        if (eb != null) {
                            ulong e = Ptr(eb, 0);
                            if (e != 0) {
                                int[] offs = { 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x90, 0x94, 0x98 };
                                foreach (int o in offs) {
                                    var ib = Read(h, pid, e + (ulong)o, 4);
                                    if (ib == null) continue;
                                    int v = BitConverter.ToInt32(ib, 0);
                                    if (v >= 0 && v < 1000) Console.WriteLine("    +0x{0:x3} = {1}", o, v);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Emit offsets file: only in-module pointers are marked valid; nulls in menu
        // may be menu-gated (need in-match re-grab) so they are commented out.
        EmitOffsets(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "offsets_generated.hpp"), clientInfo, distribute, boneBase, cameraBase,
            localIndex, viewAngles, localPos, cmdArray, activeState, gameMode, weaponDefs,
            nameArray, refDef, lootPtr, vehicleList, dump2);
        return 0;
    }

    static void EmitOffsets(string path, ulong clientInfo, ulong distribute, ulong boneBase,
        ulong cameraBase, ulong localIndex, ulong viewAngles, ulong localPos, ulong cmdArray,
        ulong activeState, ulong gameMode, ulong weaponDefs, ulong nameArray, ulong refDef,
        ulong lootPtr, ulong vehicleList, ulong cameraMat) {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("#pragma once");
        sb.AppendLine("#include <cstdint>");
        sb.AppendLine();
        sb.AppendLine("namespace Offsets {");
        sb.AppendLine("    // ============ GENERATED by graboffsets.exe ============");

        sb.AppendLine(EmitLine("CLIENT_INFO", 0x1198C7F8, IsModPtr(clientInfo) || clientInfo == 0xffffffffffffffff, " // 0xff sentinel in menu = valid"));
        sb.AppendLine(EmitLine("DISTRIBUTE", 0xA88DD10, IsModPtr(distribute), ""));
        sb.AppendLine(EmitLine("BONE_BASE", 0xCB97E48, IsModPtr(boneBase), ""));
        sb.AppendLine(EmitLine("CAMERA_MATRIX", 0xCD1031C, IsModPtr(cameraMat) || cameraMat != 0, ""));
        sb.AppendLine(EmitLine("CAMERA_BASE", 0x122B1380, IsModPtr(cameraBase), " // (chain dead/null, use CAMERA_MATRIX)"));
        sb.AppendLine(EmitLine("LOCAL_INDEX", 0x105150, IsModPtr(localIndex), ""));
        sb.AppendLine(EmitLine("VIEW_ANGLES", 0x12C00, IsModPtr(viewAngles), ""));
        sb.AppendLine(EmitLine("LOCAL_POS", 0x12C08, IsModPtr(localPos), ""));
        sb.AppendLine(EmitLine("CMD_ARRAY", 0xCD3D810, IsModPtr(cmdArray), ""));
        sb.AppendLine(EmitLine("ACTIVE_STATE", 0xF27D528, IsModPtr(activeState), ""));
        sb.AppendLine(EmitLine("GAME_MODE", 0xF07D000, IsModPtr(gameMode), ""));
        sb.AppendLine(EmitLine("WEAPON_DEFS", 0x11886930, IsModPtr(weaponDefs), ""));
        sb.AppendLine(EmitLine("NAME_ARRAY", 0x119C48F8, IsModPtr(nameArray), ""));
        sb.AppendLine(EmitLine("REF_DEF", 0x119A2DA0, IsModPtr(refDef), ""));
        sb.AppendLine(EmitLine("LOOT_PTR", 0xB8BD04C, IsModPtr(lootPtr), ""));
        sb.AppendLine(EmitLine("VEHICLE_LIST", 0x5000000, IsModPtr(vehicleList), ""));
        sb.AppendLine("}");
        File.WriteAllText(path, sb.ToString());
        Console.WriteLine("Wrote " + path);
    }

    static string EmitLine(string name, ulong off, bool ok, string note) {
        if (ok) return "    constexpr uintptr_t " + name + " = 0x" + off.ToString("X") + ";" + note;
        return "    // constexpr uintptr_t " + name + " = 0x0; // STALE - needs rediscovery" + note;
    }

    static bool IsModPtr(ulong v) { return v >= baseAddr && v < baseAddr + IMG_SIZE; }

    static uint FindPid(string name) {
        foreach (var p in System.Diagnostics.Process.GetProcesses()) {
            try { if (string.Equals(p.ProcessName + ".exe", name, StringComparison.OrdinalIgnoreCase) || string.Equals(p.ProcessName, name.Replace(".exe", ""), StringComparison.OrdinalIgnoreCase)) return (uint)p.Id; } catch { }
        }
        return 0;
    }

    [DllImport("ntdll.dll")]
    static extern int NtQueryInformationProcess(IntPtr h, int cls, out PROCESS_BASIC_INFORMATION pbi, int len, out int ret);
    [DllImport("kernel32.dll")]
    static extern IntPtr OpenProcess(uint a, bool inh, uint pid);
    [StructLayout(LayoutKind.Sequential)]
    struct PROCESS_BASIC_INFORMATION { public IntPtr Reserved1; public IntPtr PebBaseAddress; public IntPtr Reserved2_0; public IntPtr Reserved2_1; public IntPtr UniqueProcessId; public IntPtr Reserved3; }

    static ulong GetPeb(uint pid) {
        IntPtr h = OpenProcess(0x1FFFFF, false, pid);
        if (h == IntPtr.Zero) { Console.WriteLine("  OpenProcess FAIL " + Marshal.GetLastWin32Error()); return 0; }
        PROCESS_BASIC_INFORMATION pbi; int r = 0;
        int st = NtQueryInformationProcess(h, 0, out pbi, Marshal.SizeOf(typeof(PROCESS_BASIC_INFORMATION)), out r);
        return st == 0 ? (ulong)pbi.PebBaseAddress.ToInt64() : 0;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateToolhelp32Snapshot(uint flags, uint pid);
    [DllImport("kernel32.dll")]
    static extern bool Module32First(IntPtr s, ref MODULEENTRY32 m);
    [DllImport("kernel32.dll")]
    static extern bool Module32Next(IntPtr s, ref MODULEENTRY32 m);
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct MODULEENTRY32 { public uint dwSize, th32ModuleID, th32ProcessID, GlblcntUsage, ProccntUsage; public IntPtr modBaseAddr; public uint modBaseSize; public IntPtr hModule; [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string szModule; [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string szExePath; }

    static ulong FirstModuleBase(uint pid) {
        IntPtr s = CreateToolhelp32Snapshot(0x18, pid);
        if (s.ToInt64() == -1) { Console.WriteLine("  snap FAIL " + Marshal.GetLastWin32Error()); return 0; }
        MODULEENTRY32 m = new MODULEENTRY32(); m.dwSize = (uint)Marshal.SizeOf(typeof(MODULEENTRY32));
        ulong best = 0;
        int n = 0;
        if (Module32First(s, ref m)) {
            do {
                if (n < 3) Console.WriteLine("  mod[{0}] size=0x{1:x} base=0x{2:x}", n, m.modBaseSize, m.modBaseAddr.ToInt64());
                n++;
                if (m.modBaseSize == 0x2591c200 || m.modBaseSize > 0x20000000) { best = (ulong)m.modBaseAddr.ToInt64(); break; }
                if (best == 0 && m.modBaseSize >= 0x10000000) best = (ulong)m.modBaseAddr.ToInt64();
            } while (Module32Next(s, ref m));
        } else { Console.WriteLine("  Module32First FAIL " + Marshal.GetLastWin32Error()); }
        return best;
    }

    // =====================================================================
    //  REPORT FUNCTION SCAN (run in-match with --reportscan)
    //  Strategy: find report-related strings in .rdata, then find LEA RIP-relative
    //  xrefs in .text pointing at those strings -> candidate report functions.
    //  Also supports raw byte-pattern scan with --sig "48 8D 05 ?? ?? ?? ?? ..."
    // =====================================================================
    static readonly ulong RDATA_BEGIN = 0x84b2a00;
    static readonly ulong RDATA_END = 0x93a9000;
    static readonly ulong TEXT1_BEGIN = 0x1000;
    static readonly ulong TEXT1_END = 0x84b2a00;
    static readonly ulong TEXT2_BEGIN = 0x12acb000;
    static readonly ulong TEXT2_END = 0x2591c200;

    static byte[] ReadRange(IntPtr h, ulong addr, int len) {
        // read in chunks through the driver (keeps single IOCTL small)
        int chunk = 0x4000;
        var buf = new byte[len];
        int done = 0;
        while (done < len) {
            int n = Math.Min(chunk, len - done);
            var r = Read(h, pid, addr + (ulong)done, n);
            if (r == null) return null;
            Array.Copy(r, 0, buf, done, n);
            done += n;
        }
        return buf;
    }

    static ulong FindAsciiString(IntPtr h, string needle) {
        byte[] pat = System.Text.Encoding.ASCII.GetBytes(needle);
        int step = 0x4000;
        for (ulong a = RDATA_BEGIN; a < RDATA_END; a += (ulong)step) {
            int len = (int)Math.Min((ulong)step, RDATA_END - a);
            var b = ReadRange(h, a, len);
            if (b == null) continue;
            for (int i = 0; i + pat.Length <= b.Length; i++) {
                bool m = true;
                for (int k = 0; k < pat.Length; k++) if (b[i + k] != pat[k]) { m = false; break; }
                if (m) return a + (ulong)i;
            }
        }
        return 0;
    }

    static ulong RipRel(byte[] b, int i) {
        // 48 8D 05/0D/15/1D/25/2D/35/3D disp32 -> target
        byte modrm = b[i + 2];
        if ((modrm & 0xC7) != 0x05) return 0; // mod=00 rm=101 (RIP-relative), any reg
        int disp = BitConverter.ToInt32(b, i + 3);
        ulong insEnd = baseAddr + (ulong)i + 7;
        ulong target = (ulong)((long)insEnd + disp);
        return target;
    }

    static void ScanXRefs(IntPtr h, ulong strAddr, string tag) {
        Console.WriteLine("  XREFS to " + tag + " @ +0x" + (strAddr - baseAddr).ToString("x"));
        int step = 0x4000;
        int shown = 0;
        for (ulong a = TEXT1_BEGIN; a < TEXT2_END; a += (ulong)step) {
            bool inText2 = a >= TEXT2_BEGIN;
            ulong end = inText2 ? TEXT2_END : TEXT1_END;
            if (a >= end) continue;
            int len = (int)Math.Min((ulong)step, end - a);
            var b = ReadRange(h, a, len);
            if (b == null) continue;
            for (int i = 0; i + 7 < b.Length; i++) {
                if (b[i] == 0x48 && b[i + 1] == 0x8D && (b[i + 2] & 0xC0) == 0x00) {
                    ulong tgt = RipRel(b, i);
                    if (tgt == strAddr) {
                        ulong abs = a + (ulong)i;
                        Console.WriteLine("    LEA -> {0} at +0x{1} ({2})", tag, (abs - baseAddr).ToString("x"), SectionOf(abs - baseAddr));
                        shown++;
                    }
                }
            }
        }
        if (shown == 0) Console.WriteLine("    (no LEA xrefs found)");
    }

    static void ScanForPattern(IntPtr h, string sig) {
        // parse "AA BB ?? CC" -> byte list with wildcards
        var parts = sig.Split(new char[] { ' ', '\t', ',' }, StringSplitOptions.RemoveEmptyEntries);
        var bytes = new List<byte>();
        var wc = new List<bool>();
        foreach (var p in parts) {
            if (p == "??" || p == "?") { bytes.Add(0); wc.Add(true); }
            else { bytes.Add(Convert.ToByte(p, 16)); wc.Add(false); }
        }
        if (bytes.Count < 4) { Console.WriteLine("  sig too short"); return; }
        Console.WriteLine("  Pattern len=" + bytes.Count + " over .text1 + .text2");
        int step = 0x4000;
        int found = 0;
        for (ulong a = TEXT1_BEGIN; a < TEXT2_END; a += (ulong)step) {
            bool inText2 = a >= TEXT2_BEGIN;
            ulong end = inText2 ? TEXT2_END : TEXT1_END;
            if (a >= end) continue;
            int len = (int)Math.Min((ulong)step, end - a);
            var b = ReadRange(h, a, len);
            if (b == null) continue;
            for (int i = 0; i + bytes.Count <= b.Length; i++) {
                bool m = true;
                for (int k = 0; k < bytes.Count; k++) {
                    if (!wc[k] && b[i + k] != bytes[k]) { m = false; break; }
                }
                if (m) {
                    ulong abs = a + (ulong)i;
                    Console.WriteLine("    MATCH at +0x" + (abs - baseAddr).ToString("x") + " (" + SectionOf(abs - baseAddr) + ")");
                    found++;
                    i += bytes.Count;
                }
            }
        }
        if (found == 0) Console.WriteLine("    (no matches)");
    }
}

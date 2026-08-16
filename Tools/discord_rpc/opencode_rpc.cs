using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;

public class OpenCodeRPC {
    // TODO: put your real Discord Application Client ID here
    // (https://discord.com/developers -> Applications -> New Application -> copy the Application ID)
    const string CLIENT_ID = "1494304092846166016";

    const string DETAILS = "skidding through the lobby";
    const string STATE = "working on the chair still";
    const long APP_START = 0; // filled in at runtime

    static NamedPipeClientStream pipe;
    static int pipeIndex = 0;
    static long started = 0;

    static bool IsOpenCodeRunning() {
        try {
            foreach (var p in Process.GetProcesses()) {
                string name = "";
                try { name = p.ProcessName; } catch { continue; }
                if (name.IndexOf("opencode", StringComparison.OrdinalIgnoreCase) >= 0) return true;
            }
        } catch { }
        return false;
    }

    static bool Connect() {
        for (int i = 0; i < 10; i++) {
            try {
                var np = new NamedPipeClientStream(".", "discord-ipc-" + i, PipeDirection.InOut, PipeOptions.None);
                np.Connect(500);
                if (np.IsConnected) { pipe = np; pipeIndex = i; return true; }
            } catch { }
        }
        return false;
    }

    static byte[] BuildFrame(uint op, string json) {
        byte[] payload = Encoding.UTF8.GetBytes(json);
        var outBuf = new byte[8 + payload.Length];
        BitConverter.GetBytes(op).CopyTo(outBuf, 0);
        BitConverter.GetBytes((uint)payload.Length).CopyTo(outBuf, 4);
        payload.CopyTo(outBuf, 8);
        return outBuf;
    }

    static bool SendFrame(uint op, string json) {
        if (pipe == null || !pipe.IsConnected) return false;
        try {
            var frame = BuildFrame(op, json);
            pipe.Write(frame, 0, frame.Length);
            pipe.Flush();
            return true;
        } catch {
            TryClose();
            return false;
        }
    }

    static string RecvFrame(out uint op) {
        op = 0;
        if (pipe == null || !pipe.IsConnected) return null;
        try {
            var hdr = new byte[8];
            int got = 0;
            while (got < 8) { int r = pipe.Read(hdr, got, 8 - got); if (r <= 0) return null; got += r; }
            op = BitConverter.ToUInt32(hdr, 0);
            uint len = BitConverter.ToUInt32(hdr, 4);
            if (len > 65536) return null;
            var body = new byte[len];
            got = 0;
            while (got < len) { int r = pipe.Read(body, got, (int)(len - got)); if (r <= 0) return null; got += r; }
            return Encoding.UTF8.GetString(body);
        } catch {
            TryClose();
            return null;
        }
    }

    static void TryClose() {
        try { if (pipe != null) pipe.Dispose(); } catch { }
        pipe = null;
    }

    static bool DoHandshake() {
        string json = "{\"v\":1,\"client_id\":\"" + CLIENT_ID + "\"}";
        if (!SendFrame(0, json)) return false;
        uint op;
        string resp = RecvFrame(out op);
        if (resp == null) return false;
        return true; // READY received (op 1) even if we don't parse it
    }

    static void SetActivity() {
        string nonce = Guid.NewGuid().ToString();
        string json = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + Process.GetCurrentProcess().Id +
            ",\"activity\":{\"details\":\"" + DETAILS + "\",\"state\":\"" + STATE +
            "\",\"timestamps\":{\"start\":" + started + "},\"instance\":true}},\"nonce\":\"" + nonce + "\"}";
        SendFrame(1, json);
    }

    static void Main() {
        started = DateTimeOffset.UtcNow.ToUnixTimeSeconds();

        bool connected = false;
        long lastActivity = 0;
        bool wasRunning = false;

        Console.WriteLine("OpenCode RPC watcher started.");
        Console.WriteLine("Watching for opencode process...");

        while (true) {
            bool running = IsOpenCodeRunning();

            if (running) {
                if (!connected) {
                    TryClose();
                    if (Connect()) {
                        if (DoHandshake()) {
                            connected = true;
                            lastActivity = 0;
                            Console.WriteLine("Connected to Discord. Status active.");
                        } else {
                            TryClose();
                        }
                    }
                }
                if (connected) {
                    long now = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                    if (now - lastActivity > 15000) {
                        SetActivity();
                        lastActivity = now;
                    }
                }
            } else {
                if (connected) {
                    TryClose();
                    connected = false;
                    Console.WriteLine("opencode not running. Status cleared.");
                }
            }

            if (running != wasRunning) {
                wasRunning = running;
                if (running) Console.WriteLine("opencode detected.");
                else Console.WriteLine("opencode stopped.");
            }

            Thread.Sleep(2000);
        }
    }
}

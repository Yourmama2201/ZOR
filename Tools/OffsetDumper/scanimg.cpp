#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// offline signature scanner for full_image.bin (file offset = vaddr)
// sections (from fulldump.cs): file offset == VirtualAddress

struct Sig { const char* name; const char* hex; int extra; bool rel; };

static Sig sigs[] = {
    {"CAMERA_BASE", "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? 48 85 C9", 3, true},
    {"DISTRIBUTE", "48 8B 05 ? ? ? ? 48 8B 48 ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ?", 3, true},
    {"BONE_BASE", "48 8B 05 ? ? ? ? 48 8B 08 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88", 3, true},
    {"VIEW_ANGLES", "F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? 48 83 C4 ? 5B C3", 3, true},
    {"LOCAL_POS", "F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? F3 0F 10 0D ? ? ? ?", 3, true},
    {"CMD_ARRAY", "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ?", 3, true},
    {"WEAPON_DEFS", "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 48 8D 55", 3, true},
    {"ACTIVE_STATE", "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? E8", 3, true},
    {"NAME_ARRAY", "48 8B 05 ? ? ? ? 48 8D 14 80 48 8B 04 D0 48 85 C0 74 ?", 3, true},
    {"GAME_MODE", "83 3D ? ? ? ? ? 75 ? 48 8D 15 ? ? ? ? 48 8D 0D ? ? ? ? E8", 2, true},
    {"LOOT_PTR", "48 8B 0D ? ? ? ? 48 8B 49 ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ?", 3, true},
    {"REF_DEF", "48 8B 05 ? ? ? ? 48 8D 0C 80 48 8B 04 C8 48 85 C0 74 ?", 3, true},
    {"CLIENT_INFO", "48 8B 05 ? ? ? ? 48 33 05 ? ? ? ? 48 8D 0D ? ? ? ? 48 89 05", 3, true},
    {"LOCAL_INDEX", "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 80 ? ? ? ? 48 8B 5C 24 ?", 3, true},
    {"TIMESTAMP", "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 ? 8B 80 ? ? ? ? C3", 3, true},
    {"WEAPON_INIT", "48 8B 05 ? ? ? ? 48 8B 08 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? 48 85 C9", 3, true},
};

struct Pattern {
    std::string name;
    std::vector<uint8_t> pat, mask;
    int extra; bool rel;
};

static Pattern makeSig(const Sig& s) {
    Pattern p; p.name = s.name; p.extra = s.extra; p.rel = s.rel;
    std::string h = s.hex; size_t pos = 0;
    while (pos < h.size()) {
        while (pos < h.size() && h[pos] == ' ') pos++;
        if (pos >= h.size()) break;
        if (h[pos] == '?') { p.pat.push_back(0); p.mask.push_back(0); pos++; }
        else {
            uint8_t b = 0;
            for (int i = 0; i < 2; i++) {
                char c = h[pos++]; b <<= 4;
                if (c >= '0' && c <= '9') b |= (c - '0');
                else if (c >= 'A' && c <= 'F') b |= (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') b |= (c - 'a' + 10);
            }
            p.pat.push_back(b); p.mask.push_back(1);
        }
    }
    return p;
}

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "full_image.bin";
    FILE* f = fopen(path, "rb");
    if (!f) { printf("ERR open %s\n", path); return 1; }
    fseek(f, 0, SEEK_END); long long sz = _ftelli64(f); fseek(f, 0, SEEK_SET);
    printf("file %s size=%lld\n", path, sz);
    std::vector<uint8_t> buf(sz);
    size_t rd = fread(buf.data(), 1, sz, f);
    fclose(f);
    printf("read %zu bytes\n", rd);

    // sections (file offset == vaddr), VA deltas for relative resolution
    struct Sect { uint32_t vaddr, vsize; };
    std::vector<Sect> sects = {
        {0x1000, 0x84b2a00},   // .text 1
        {0x84b4000, 0xef5000}, // .rdata
        {0x93a9000, 0x92d7bcc},// .data
        {0x12681000, 0x225400},// .pdata
        {0x128a7000, 0xa00},
        {0x128a8000, 0x14200},
        {0x128bd000, 0x1b4400},
        {0x12a72000, 0x58400},
        {0x12acb000, 0x12e51200}, // .text 2
    };

    const uintptr_t BASE = 0x7ff64eaa0000ull;
    std::vector<Pattern> pats;
    for (auto& s : sigs) pats.push_back(makeSig(s));

    int found = 0;
    for (auto& p : pats) {
        bool any = false;
        for (auto& se : sects) {
            uint32_t end = se.vaddr + se.vsize;
            if (se.vaddr + p.pat.size() > buf.size()) continue;
            for (uint32_t pos = se.vaddr; pos + p.pat.size() <= end && pos < buf.size(); pos++) {
                bool ok = true;
                for (size_t i = 0; i < p.pat.size(); i++)
                    if (p.mask[i] && buf[pos + i] != p.pat[i]) { ok = false; break; }
                if (ok) {
                    uintptr_t addr = BASE + pos;
                    uintptr_t result = addr;
                    if (p.rel && p.extra >= 0) {
                        int32_t rel = 0;
                        memcpy(&rel, buf.data() + pos + p.extra, 4);
                        result = addr + p.extra + 4 + rel;
                    }
                    printf("  FOUND %-14s off=0x%llX  (%s section, VA 0x%llX)\n",
                        p.name.c_str(), (unsigned long long)(result - BASE),
                        (pos >= 0x12acb000 ? ".text2" : pos >= 0x93a9000 && pos < 0x12681000 ? ".data" : pos >= 0x84b4000 && pos < 0x93a9000 ? ".rdata" : ".text1"),
                        (unsigned long long)result);
                    found++; any = true;
                    break;
                }
            }
        }
        if (!any) printf("  miss  %-14s\n", p.name.c_str());
    }
    printf("DONE: %d/%d found\n", found, (int)pats.size());
    return 0;
}
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

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

// generic PE-section scan: arg1=file, arg2=module base (hex)
int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "dump.bin";
    uintptr_t BASE = argc > 2 ? strtoull(argv[2], nullptr, 16) : 0;
    FILE* f = fopen(path, "rb");
    if (!f) { printf("ERR open %s\n", path); return 1; }
    fseek(f, 0, SEEK_END); long long sz = _ftelli64(f); fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(sz);
    size_t rd = fread(buf.data(), 1, sz, f);
    fclose(f);
    printf("file %s size=%lld base=0x%llX\n", path, sz, (unsigned long long)BASE);

    if (buf.size() < 0x1000 || buf[0] != 'M' || buf[1] != 'Z') { printf("ERR not a PE\n"); return 1; }
    int pe = *(int*)(buf.data() + 0x3C);
    uint16_t numSections = *(uint16_t*)(buf.data() + pe + 6);
    uint16_t optSize = *(uint16_t*)(buf.data() + pe + 0x14);
    printf("sections=%u optSize=%u\n", numSections, optSize);
    size_t sh = pe + 4 + 20 + optSize;
    struct Sect { uint32_t vaddr, vsize; };
    std::vector<Sect> sects;
    for (int i = 0; i < numSections; i++) {
        uint32_t vaddr = *(uint32_t*)(buf.data() + sh + (size_t)i * 40 + 12);
        uint32_t vsize = *(uint32_t*)(buf.data() + sh + (size_t)i * 40 + 8);
        if (vaddr && vsize) sects.push_back({ vaddr, vsize });
    }
    for (auto& se : sects) printf("  sect VA=0x%X VSize=0x%X\n", se.vaddr, se.vsize);

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
                    printf("  FOUND %-14s off=0x%llX  VA 0x%llX\n",
                        p.name.c_str(), (unsigned long long)(result - BASE),
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
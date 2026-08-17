#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("ERR\n"); return 1; }
    fseek(f, 0, SEEK_END); long long sz = _ftelli64(f); fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(sz);
    size_t rd = fread(buf.data(), 1, sz, f);
    fclose(f);
    long long nz = 0;
    for (size_t i = 0; i < rd; i++) if (buf[i]) nz++;
    printf("%s: size=%lld read=%zu nonZero=%lld pct=%.2f\n",
        argv[1], sz, rd, nz, 100.0 * (double)nz / (double)(rd ? rd : 1));
    // first real bytes
    size_t i = 0;
    while (i < rd && buf[i] == 0) i++;
    printf("first non-zero byte at 0x%zX: %02X %02X %02X %02X\n", i,
        buf[i], buf[i+1], buf[i+2], buf[i+3]);
    return 0;
}
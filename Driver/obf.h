#pragma once
// Compile-time XOR string obfuscation.
// Strings are stored encrypted in .rdata and decoded into a stack buffer at
// runtime, so identifying strings (device names, module tags, pool tags) never
// appear as plaintext in the image.

#include <ntifs.h>

#define SO_KEY 0x5A
static volatile unsigned char g_SOKey = SO_KEY;

// Encrypts a literal at compile time.
#define SO_CH(c) ((c) ^ SO_KEY)

// Build an encrypted byte array from a literal (including terminator).
#define SO_BLOCK(lit) \
    (const unsigned char[]){ SO_CH((lit)[0]), SO_CH((lit)[1]), SO_CH((lit)[2]), SO_CH((lit)[3]), \
                             SO_CH((lit)[4]), SO_CH((lit)[5]), SO_CH((lit)[6]), SO_CH((lit)[7]), \
                             SO_CH((lit)[8]), SO_CH((lit)[9]), SO_CH((lit)[10]), SO_CH((lit)[11]), \
                             SO_CH((lit)[12]), SO_CH((lit)[13]), SO_CH((lit)[14]), SO_CH((lit)[15]), \
                             SO_CH((lit)[16]), SO_CH((lit)[17]), SO_CH((lit)[18]), SO_CH((lit)[19]), \
                             SO_CH((lit)[20]), SO_CH((lit)[21]), SO_CH((lit)[22]), SO_CH((lit)[23]), \
                             SO_CH((lit)[24]), SO_CH((lit)[25]), SO_CH((lit)[26]), SO_CH((lit)[27]), \
                             SO_CH((lit)[28]), SO_CH((lit)[29]), SO_CH((lit)[30]), SO_CH((lit)[31]), \
                             SO_CH((lit)[32]), SO_CH((lit)[33]), SO_CH((lit)[34]), SO_CH((lit)[35]), \
                             SO_CH((lit)[36]), SO_CH((lit)[37]), SO_CH((lit)[38]), SO_CH((lit)[39]), \
                             SO_CH((lit)[40]), SO_CH((lit)[41]), SO_CH((lit)[42]), SO_CH((lit)[43]), \
                             SO_CH((lit)[44]), SO_CH((lit)[45]), SO_CH((lit)[46]), SO_CH((lit)[47]), \
                             SO_CH((lit)[48]), SO_CH((lit)[49]), SO_CH((lit)[50]), SO_CH((lit)[51]), \
                             SO_CH((lit)[52]), SO_CH((lit)[53]), SO_CH((lit)[54]), SO_CH((lit)[55]), \
                             SO_CH((lit)[56]), SO_CH((lit)[57]), SO_CH((lit)[58]), SO_CH((lit)[59]), \
                             SO_CH((lit)[60]), SO_CH((lit)[61]), SO_CH((lit)[62]), SO_CH((lit)[63]), \
                             0 }

// Decode into a stack buffer. Buffer must be >= strlen+1.
// The key is read from a volatile global and the block through a volatile
// pointer, so the optimizer cannot constant-fold the XOR back into the
// plaintext string in the final image.
#define SO_DEC(buf, block) \
    { const volatile unsigned char* _s = (block); \
      volatile unsigned char _k = g_SOKey; \
      for (int _i = 0; _i < (int)sizeof(block); _i++) { buf[_i] = (char)(_s[_i] ^ _k); } }
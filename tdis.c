/* tdis.c - disassembler for tvm.  usage: tdis file.tvb [start [end]]
 * Linear sweep: data bytes show up as .byte lines or as nonsense instructions; give start/end to skip them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tvm.h"

static const char *name[256];
static int kind[256];

int main(int argc, char **argv)
{
    static uint8_t m[65536 + 8];
    FILE *f; size_t n; long pc, end, a, v; int op, k, i;
    if (argc < 2) { fprintf(stderr, "usage: tdis file.tvb [start [end]]\n"); return 2; }
    for (i = 0; i < 256; i++) kind[i] = -1;
#define X(n,c,k) name[c] = #n; kind[c] = K_##k;
    TVM_OPS(X)
#undef X
    if (!(f = fopen(argv[1], "rb"))) { perror(argv[1]); return 1; }
    n = fread(m, 1, 65536, f);
    fclose(f);
    if (n && m[0] == TVM_MAGIC) {                   /* skip the header, show the entry point */
        u32 ent = 0; int s = 0, b; size_t off = 1;
        while (off < n) { b = m[off++]; ent |= (u32)(b & 127) << s; s += 7; if (!(b & 128) || s >= 21) break; }
        printf("; entry 0x%04x\n", (unsigned)ent);
        memmove(m, m + off, n - off);
        n -= off;
    }
    pc = argc > 2 ? strtol(argv[2], 0, 0) : 0;
    end = argc > 3 ? strtol(argv[3], 0, 0) : (long)n;
    if (end > (long)n) end = (long)n;
    while (pc < end) {
        a = pc; op = m[pc++]; k = kind[op]; v = 0;
        if (k < 0) { printf("%04lx: %02x                .byte 0x%02x\n", a, op, op); continue; }
        if (k == K_I8) v = (int8_t)m[pc++];
        else if (k == K_U8) v = m[pc++];
        else if (k == K_V || k == K_R) {
            u32 z = 0; int s = 0, b;
            do { b = m[pc++]; z |= (u32)(b & 127) << s; s += 7; } while ((b & 128) && s < 35);
            v = (i32)(z >> 1) ^ -(i32)(z & 1);
        }
        printf("%04lx: ", a);
        for (i = 0; i < 6; i++) { if (a + i < pc) printf("%02x ", m[a + i]); else printf("   "); }
        if (k == K_N) printf(" %s\n", name[op]);
        else if (k == K_R) printf(" %-7s 0x%04lx\n", name[op], (pc + v) & 0xFFFF);
        else printf(" %-7s %ld\n", name[op], v);
    }
    return 0;
}

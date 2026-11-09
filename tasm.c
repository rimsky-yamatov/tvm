/* tasm.c - assembler for tvm.  usage: tasm in.asm out.bin
 *
 * syntax:  label:  MNEMONIC operand   ; comment
 *   PUSH x            picks PUSH0 / PUSH1 / PUSHI8 / PUSHV automatically
 *   .equ NAME value   .byte a,b,..   .word a,..(32-bit LE)   .space n   .str "text\n" (NUL-terminated)
 *   operands: 123  0x1F  'A'  label  label+4  NAME
 * Variable-length encodings are resolved by repeated passes; sizes only grow, so it always converges.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tvm.h"

typedef struct { const char *n; int c, k; } Op;
static const Op OPT[] = {
#define X(n,c,k) { #n, c, K_##k },
    TVM_OPS(X)
#undef X
};
#define NOPS ((int)(sizeof OPT / sizeof OPT[0]))

typedef struct { char n[40]; long v; int def, dp; } Sym;
static Sym sy[2048];
static int nsy;
static uint8_t out[65536];
static long pc;
static uint8_t sz[32768];               /* sticky encoded size per variable-length instruction */
static int ii, changed, pass, ln;
static const char *fname;

static void die(const char *m, const char *a)
{
    fprintf(stderr, "%s:%d: %s%s%s\n", fname, ln, m, a ? ": " : "", a ? a : "");
    exit(1);
}

static Sym *sym(const char *n)
{
    int i;
    for (i = 0; i < nsy; i++) if (!strcmp(sy[i].n, n)) return &sy[i];
    if (nsy >= 2048 || strlen(n) >= 40) die("too many or too long symbols", n);
    strcpy(sy[nsy].n, n);
    return &sy[nsy++];
}

static void deflab(const char *n, long v)
{
    Sym *y = sym(n);
    if (y->def && y->dp == pass) die("duplicate definition", n);
    if (!y->def || y->v != v) changed = 1;
    y->def = 1; y->dp = pass; y->v = v;
}

static int esc(int c) { switch (c) { case 'n': return 10; case 't': return 9; case 'r': return 13; case '0': return 0; default: return c; } }

static long term(const char **p)
{
    const char *s = *p; long v = 0; int neg = 0;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '-') { neg = 1; s++; }
    if (*s == '\'') {
        s++;
        if (*s == '\\') { s++; v = esc(*s); s++; } else v = (unsigned char)*s++;
        if (*s != '\'') die("bad character literal", 0);
        s++;
    } else if (isdigit((unsigned char)*s)) {
        char *e; v = strtol(s, &e, 0); s = e;
    } else if (isalpha((unsigned char)*s) || *s == '_' || *s == '.') {
        char n[40]; int i = 0;
        while ((isalnum((unsigned char)*s) || *s == '_' || *s == '.') && i < 39) n[i++] = *s++;
        n[i] = 0;
        v = sym(n)->v;                  /* unresolved symbols read as 0 until a later pass */
    } else die("bad operand", *p);
    *p = s;
    return neg ? -v : v;
}

static long expr(const char **p)
{
    long v = term(p);
    for (;;) {
        while (isspace((unsigned char)**p)) (*p)++;
        if (**p == '+') { (*p)++; v += term(p); }
        else if (**p == '-') { (*p)++; v -= term(p); }
        else return v;
    }
}

static void eol(const char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (*s) die("unexpected text", s);
}

static void eb(int b) { if (pc < 65536) out[pc] = (uint8_t)b; pc++; }

static u32 zz(long v) { return ((u32)v << 1) ^ (u32)((i32)v >> 31); }
static int vlen(long v) { u32 z = zz(v); int n = 1; while (z >>= 7) n++; return n; }
static void ev(long v, int len)         /* zigzag LEB128, zero-padded up to len bytes */
{
    u32 z = zz(v); int i;
    for (i = 0; i < len; i++) { eb((int)(z & 127) | (i < len - 1 ? 128 : 0)); z >>= 7; }
}

static int stick(int need)
{
    int r;
    if (ii >= (int)sizeof sz) die("program too large", 0);
    if (need > sz[ii]) { sz[ii] = (uint8_t)need; changed = 1; }
    r = sz[ii++];
    return r;
}

static void push(long v)
{
    int need = (v == 0 || v == 1) ? 1 : (v >= -128 && v <= 127) ? 2 : 1 + vlen(v);
    int s = stick(need);
    if (s == 1) eb(v ? OP_PUSH1 : OP_PUSH0);
    else if (s == 2) { eb(OP_PUSHI8); eb((int)(v & 255)); }
    else { eb(OP_PUSHV); ev(v, s - 1); }
}

static void rel(int opc, long t)
{
    int len = sz[ii] ? sz[ii] : 1, n;
    long r;
    for (;;) { n = vlen(t - (pc + 1 + len)); if (n <= len) break; len = n; }
    stick(len);
    r = t - (pc + 1 + len);
    eb(opc); ev(r, len);
}

static void ins(const Op *o, const char *a)
{
    long v; int s;
    switch (o->k) {
    case K_N:  eb(o->c); break;
    case K_U8: v = expr(&a); if (v < 0 || v > 255) die("operand out of range (0..255)", 0); eb(o->c); eb((int)v); break;
    case K_I8: v = expr(&a); if (v < -128 || v > 127) die("operand out of range (-128..127)", 0); eb(o->c); eb((int)(v & 255)); break;
    case K_V:  v = expr(&a); s = stick(vlen(v)); eb(o->c); ev(v, s); break;
    case K_R:  v = expr(&a); rel(o->c, v); break;
    }
    eol(a);
}

static void line(char *s)
{
    char *p, *q, *m, u[16]; size_t i;
    { int qt = 0;                                       /* strip ; comments (not inside "..." or '.') */
      for (p = s; *p; p++) {
        if (*p == '"' && (p == s || p[-1] != '\\')) qt = !qt;
        else if (!qt && *p == '\'' && p[1] == '\\' && p[3] == '\'') p += 3;
        else if (!qt && *p == '\'' && p[1] && p[2] == '\'') p += 2;
        else if (!qt && *p == ';') { *p = 0; break; }
      } }
    for (;;) {                                          /* labels */
        while (isspace((unsigned char)*s)) s++;
        for (q = s; isalnum((unsigned char)*q) || *q == '_' || *q == '.'; q++) ;
        if (q > s && *q == ':') {
            char n[40]; size_t l = (size_t)(q - s);
            if (l >= 40) die("label too long", 0);
            memcpy(n, s, l); n[l] = 0;
            deflab(n, pc);
            s = q + 1;
            continue;
        }
        break;
    }
    if (!*s) return;
    m = s;
    while (*s && !isspace((unsigned char)*s)) s++;
    if (*s) *s++ = 0;
    while (isspace((unsigned char)*s)) s++;

    if (m[0] == '.') {
        if (!strcmp(m, ".str")) {
            if (*s != '"') die("string expected", 0);
            for (s++; *s && *s != '"'; s++) { int ch = (unsigned char)*s; if (ch == '\\' && s[1]) ch = esc(*++s); eb(ch); }
            if (*s != '"') die("unterminated string", 0);
            eb(0);
        } else if (!strcmp(m, ".byte") || !strcmp(m, ".word")) {
            int w = m[1] == 'w';
            for (;;) {
                long v = expr((const char **)&s);
                eb((int)(v & 255));
                if (w) { eb((int)((v >> 8) & 255)); eb((int)((v >> 16) & 255)); eb((int)((v >> 24) & 255)); }
                while (isspace((unsigned char)*s)) s++;
                if (*s == ',') { s++; continue; }
                break;
            }
            eol(s);
        } else if (!strcmp(m, ".space")) {
            long n = expr((const char **)&s);
            while (n-- > 0) eb(0);
            eol(s);
        } else if (!strcmp(m, ".equ")) {
            char n[40]; int k = 0;
            while ((isalnum((unsigned char)*s) || *s == '_') && k < 39) n[k++] = *s++;
            n[k] = 0;
            if (!k) die("name expected", 0);
            while (isspace((unsigned char)*s) || *s == ',') s++;
            deflab(n, expr((const char **)&s));
            eol(s);
        } else die("unknown directive", m);
        return;
    }
    for (i = 0; m[i] && i < 15; i++) u[i] = (char)toupper((unsigned char)m[i]);
    u[i] = 0;
    if (!strcmp(u, "PUSH")) { push(expr((const char **)&s)); eol(s); return; }
    for (i = 0; i < (size_t)NOPS; i++) if (!strcmp(u, OPT[i].n)) { ins(&OPT[i], s); return; }
    die("unknown mnemonic", m);
}

int main(int argc, char **argv)
{
    FILE *f; char *buf, tmp[512], *b, *e; long n; size_t l; int i, k;
    if (argc < 3) { fprintf(stderr, "usage: tasm in.asm out.bin\n"); return 2; }
    fname = argv[1];
    if (!(f = fopen(fname, "rb"))) { perror(fname); return 1; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + 1);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "read error\n"); return 1; }
    buf[n] = 0; fclose(f);

    for (pass = 0; pass < 100; pass++) {
        changed = 0; pc = 0; ii = 0; ln = 0;
        for (b = buf; *b; b += l + (e ? 1 : 0)) {
            e = strchr(b, '\n');
            l = e ? (size_t)(e - b) : strlen(b);
            if (l >= sizeof tmp) { ln++; die("line too long", 0); }
            memcpy(tmp, b, l); tmp[l] = 0;
            if (l && tmp[l-1] == '\r') tmp[l-1] = 0;
            ln++;
            line(tmp);
        }
        if (pc > 65536) { ln = 0; die("program exceeds 64KB", 0); }
        if (pass > 0 && !changed) break;
    }
    ln = 0;
    if (changed) die("did not converge", 0);
    for (k = 0; k < nsy; k++) if (!sy[k].def) die("undefined symbol", sy[k].n);
    if (!(f = fopen(argv[2], "wb"))) { perror(argv[2]); return 1; }
    fwrite(out, 1, (size_t)pc, f);
    fclose(f);
    (void)i;
    fprintf(stderr, "tasm: %ld bytes\n", pc);
    return 0;
}

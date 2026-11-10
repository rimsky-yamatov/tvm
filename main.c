/* main.c - host: loads a binary, runs it, implements SYS calls (console + GUI). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
# include <io.h>
# include <fcntl.h>
#endif
#include "tvm.h"
#include "gui.h"

static VM vm;
static int exited, exit_code;
static int cur = -1;                    /* currently selected window */
static u32 rng = 2463534242u;
#define PI 3.14159265358979323846

static u32 isqrt(u32 n)
{
    u32 r = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) { if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return r;
}

static int cl(u32 x) { i32 n = (i32)x; return n < -32768 ? -32768 : n > 32767 ? 32767 : n; }
static int cr(u32 x) { i32 n = (i32)x; return n < 0 ? -1 : n > 2048 ? 2048 : n; }   /* radius */

static void getstr(VM *v, u32 p, char *buf, int n)
{
    int i;
    for (i = 0; i < n - 1; i++) { char ch = (char)v->mem[(uint16_t)(p + i)]; if (!ch) break; buf[i] = ch; }
    buf[i] = 0;
}

/* ---- sandboxed file I/O: only active with  tvm -fs DIR  ---- */
static FILE *files[8];
static const char *fsroot;              /* NULL = disabled */

/* relative paths only: no absolute paths, drive letters, backslashes or ".." components */
static int safe_rel(const char *p)
{
    const char *q; size_t n = strlen(p);
    if (!n || n > 200 || p[0] == '/') return 0;
    for (q = p; *q; q++) if (*q == '\\' || *q == ':') return 0;
    for (q = p; *q; ) {
        if (q[0] == '.' && q[1] == '.' && (q[2] == '/' || q[2] == 0)) return 0;
        while (*q && *q != '/') q++;
        if (*q == '/') q++;
    }
    return 1;
}

static int resolve(VM *v, u32 pathp, char *full, size_t n)
{
    char rel[256];
    if (!fsroot) return 0;
    getstr(v, pathp, rel, sizeof rel);
    if (!safe_rel(rel)) return 0;
    snprintf(full, n, "%s/%s", fsroot, rel);
    return 1;
}

static FILE *H(u32 h) { return h < 8 ? files[h] : NULL; }

static i32 f_open(VM *v, u32 pathp, u32 mode)
{
    static const char *m[] = { "rb", "wb", "ab", "r+b" };
    char full[1100]; int i;
    if (mode > 3 || !resolve(v, pathp, full, sizeof full)) return -1;
    for (i = 0; i < 8 && files[i]; i++) ;
    if (i == 8) return -1;
    files[i] = fopen(full, m[mode]);
    return files[i] ? i : -1;
}

static i32 f_close(u32 h) { FILE *f = H(h); if (!f) return -1; files[h] = NULL; return fclose(f) ? -1 : 0; }

static i32 f_read(VM *v, u32 h, u32 p, u32 n)
{
    static uint8_t tmp[65536]; FILE *f = H(h); size_t r, i;
    if (!f) return -1;
    if (n > 65536) n = 65536;
    r = fread(tmp, 1, n, f);
    for (i = 0; i < r; i++) v->mem[(uint16_t)(p + i)] = tmp[i];
    return (i32)r;
}

static i32 f_write(VM *v, u32 h, u32 p, u32 n)
{
    static uint8_t tmp[65536]; FILE *f = H(h); size_t i;
    if (!f) return -1;
    if (n > 65536) n = 65536;
    for (i = 0; i < n; i++) tmp[i] = v->mem[(uint16_t)(p + i)];
    return (i32)fwrite(tmp, 1, n, f);
}

static i32 f_size(u32 h)
{
    FILE *f = H(h); long cur, sz;
    if (!f) return -1;
    cur = ftell(f); fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, cur, SEEK_SET);
    return (i32)sz;
}

/* Arguments are pushed left to right, so the last argument is on top of the stack. */
int tvm_sys(VM *v, int n)
{
    u32 a, b, c, d, e, f; char buf[256]; int w, t, x, y;
    switch (n) {
    case 0:  putchar((int)(vpop(v) & 255)); break;                       /* PUTC(c)            */
    case 1:  printf("%d", (int)vpop(v)); break;                          /* PUTI(n)            */
    case 2:  fflush(stdout); vpush(v, (u32)getchar()); break;           /* GETC -> byte or -1 at EOF */
    case 10: b = vpop(v); a = vpop(v); cur = gui_open(cl(a), cl(b)); vpush(v, (u32)cur); break; /* WIN(w,h)->id */
    case 11: cur = (int)vpop(v); break;                                  /* WSEL(id)           */
    case 12: a = vpop(v); gui_close((int)a); if ((int)a == cur) cur = -1; break; /* WCLOSE(id)  */
    case 13: getstr(v, vpop(v), buf, sizeof buf); gui_title(cur, buf); break;    /* WTITLE(str) */
    case 14: b = vpop(v); a = vpop(v); vpush(v, (u32)gui_getpixel(cur, cl(a), cl(b))); break;   /* GETPIXEL(x,y) -> c or -1 */
    case 15: f = vpop(v); e = vpop(v); d = vpop(v); c = vpop(v); b = vpop(v); a = vpop(v);   /* BLIT(x,y,w,h,ptr,key) */
             { static uint8_t px[65536]; int bw = cl(c), bh = cl(d), i, k = bw * bh;
               if (bw <= 0 || bh <= 0 || k > 65536) break;
               for (i = 0; i < k; i++) px[i] = v->mem[(uint16_t)(e + (u32)i)];
               gui_blit(cur, cl(a), cl(b), bw, bh, px, (int)f); }
             break;
    case 16: d = vpop(v); c = vpop(v); b = vpop(v); a = vpop(v);
             gui_circle(cur, cl(a), cl(b), cr(c), (int)(d & 255)); break;                       /* CIRCLE(x,y,r,c)  */
    case 17: d = vpop(v); c = vpop(v); b = vpop(v); a = vpop(v);
             gui_fcircle(cur, cl(a), cl(b), cr(c), (int)(d & 255)); break;                      /* FCIRCLE(x,y,r,c) */
    case 18: e = vpop(v); d = vpop(v); c = vpop(v); b = vpop(v); a = vpop(v);
             gui_recto(cur, cl(a), cl(b), cl(c), cl(d), (int)(e & 255)); break;                 /* RECTO(x,y,w,h,c) */
    case 20: c = vpop(v); b = vpop(v); a = vpop(v); gui_pixel(cur, cl(a), cl(b), (int)(c & 255)); break;
    case 21: e = vpop(v); d = vpop(v); c = vpop(v); b = vpop(v); a = vpop(v);
             gui_rect(cur, cl(a), cl(b), cl(c), cl(d), (int)(e & 255)); break;       /* RECT(x,y,w,h,c) */
    case 22: e = vpop(v); d = vpop(v); c = vpop(v); b = vpop(v); a = vpop(v);
             gui_line(cur, cl(a), cl(b), cl(c), cl(d), (int)(e & 255)); break;       /* LINE(x0,y0,x1,y1,c) */
    case 23: d = vpop(v); c = vpop(v); b = vpop(v); a = vpop(v); getstr(v, c, buf, sizeof buf);
             gui_text(cur, cl(a), cl(b), buf, (int)(d & 255)); break;                /* TEXT(x,y,str,c) */
    case 24: gui_clear(cur, (int)(vpop(v) & 255)); break;                /* CLEAR(c)           */
    case 25: b = vpop(v); a = vpop(v); gui_pal((int)(a & 255), b); break;/* PAL(i,rgb)         */
    case 26: gui_flip(cur); break;                                       /* FLIP               */
    case 30: gui_poll(&w, &t, &x, &y);                                   /* POLL -> win,type,a,b */
             vpush(v, (u32)w); vpush(v, (u32)t); vpush(v, (u32)x); vpush(v, (u32)y); break;
    case 34: gui_mouse(&w, &x, &y, &t);                                  /* MOUSE -> win,x,y,buttons */
             vpush(v, (u32)w); vpush(v, (u32)x); vpush(v, (u32)y); vpush(v, (u32)t); break;
    case 35: a = vpop(v); vpush(v, (u32)gui_key((int)a)); break;         /* KEY(code) -> 0/1   */
    case 31: vpush(v, gui_ticks()); break;                               /* TICK -> ms         */
    case 32: gui_sleep((int)vpop(v)); break;                             /* SLEEP(ms)          */
    case 33: a = vpop(v); rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
             vpush(v, a ? rng % a : 0); break;                           /* RND(n) -> 0..n-1   */
    case 40: a = vpop(v); vpush(v, (u32)(i32)lround(sin((double)(i32)a * PI / 180.0) * 65536.0)); break;  /* SIN(deg) -> 16.16 */
    case 41: a = vpop(v); vpush(v, (u32)(i32)lround(cos((double)(i32)a * PI / 180.0) * 65536.0)); break;  /* COS(deg) -> 16.16 */
    case 42: a = vpop(v); vpush(v, (i32)a < 0 ? 0 : isqrt(a)); break;                                     /* SQRT(n)           */
    case 50: b = vpop(v); a = vpop(v); gui_beep((int)a, (int)b); break;      /* BEEP(freq,ms)     */
    case 60: b = vpop(v); a = vpop(v); vpush(v, (u32)f_open(v, a, b)); break;           /* FOPEN(path,mode) -> h/-1; mode 0 r,1 w,2 a,3 r+ */
    case 61: a = vpop(v); vpush(v, (u32)f_close(a)); break;                             /* FCLOSE(h) -> 0/-1    */
    case 62: c = vpop(v); b = vpop(v); a = vpop(v); vpush(v, (u32)f_read(v, a, b, c)); break;   /* FREAD(h,ptr,n) -> count */
    case 63: c = vpop(v); b = vpop(v); a = vpop(v); vpush(v, (u32)f_write(v, a, b, c)); break;  /* FWRITE(h,ptr,n) -> count */
    case 64: a = vpop(v); vpush(v, H(a) ? (u32)fgetc(H(a)) : (u32)-1); break;          /* FGETC(h) -> byte/-1  */
    case 65: b = vpop(v); a = vpop(v); vpush(v, H(a) ? (u32)fputc((int)(b & 255), H(a)) : (u32)-1); break;   /* FPUTC(h,c) */
    case 66: b = vpop(v); a = vpop(v); vpush(v, H(a) && fseek(H(a), (long)(i32)b, SEEK_SET) == 0 ? 0u : (u32)-1); break; /* FSEEK(h,pos) */
    case 67: a = vpop(v); vpush(v, (u32)f_size(a)); break;                              /* FSIZE(h) -> bytes/-1 */
    case 68: a = vpop(v); { char full[1100]; vpush(v, resolve(v, a, full, sizeof full) && remove(full) == 0 ? 0u : (u32)-1); } break; /* FREMOVE(path) */
    case 99: exit_code = (int)vpop(v); exited = 1; return 1;                                                   /* EXIT               */
    }
    return 0;
}

int main(int argc, char **argv)
{
    static uint8_t img[MEMSZ + 16];
    FILE *f; size_t n, off = 0; u32 entry = 0; int rc, ai = 1, i;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);     /* no CRLF translation: same bytes on every OS */
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argc > 3 && !strcmp(argv[1], "-fs")) { fsroot = argv[2]; ai = 3; }     /* enable file I/O below DIR */
    if (argc <= ai) { fprintf(stderr, "usage: tvm [-fs DIR] prog.tvb   (env TVM_SCALE=n for window zoom)\n"); return 2; }
    if (!(f = fopen(argv[ai], "rb"))) { perror(argv[ai]); return 1; }
    n = fread(img, 1, sizeof img, f);
    fclose(f);
    if (n && img[0] == TVM_MAGIC) {                 /* header: [0xB1][entry ULEB128] */
        int s = 0, b;
        for (off = 1; off < n; ) { b = img[off++]; entry |= (u32)(b & 127) << s; s += 7; if (!(b & 128) || s >= 21) break; }
    }
    if (n <= off) { fprintf(stderr, "tvm: empty program\n"); return 1; }
    n -= off;
    if (n > MEMSZ) n = MEMSZ;
    memcpy(vm.mem, img + off, n);
    vm.entry = (uint16_t)entry;
    rc = tvm_run(&vm);
    if (rc && !exited) fprintf(stderr, "tvm: invalid opcode\n");
    for (i = 0; i < 8; i++) if (files[i]) fclose(files[i]);
    gui_quit();
    return exited ? exit_code : rc;
}

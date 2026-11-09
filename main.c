/* main.c - host: loads a binary, runs it, implements SYS calls (console + GUI). */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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
    case 99: exit_code = (int)vpop(v); exited = 1; return 1;                                                   /* EXIT               */
    }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f; size_t n; int rc;
    if (argc < 2) { fprintf(stderr, "usage: tvm prog.bin   (env TVM_SCALE=n for window zoom)\n"); return 2; }
    if (!(f = fopen(argv[1], "rb"))) { perror(argv[1]); return 1; }
    n = fread(vm.mem, 1, MEMSZ, f);
    fclose(f);
    if (!n) { fprintf(stderr, "tvm: empty program\n"); return 1; }
    rc = tvm_run(&vm);
    if (rc && !exited) fprintf(stderr, "tvm: invalid opcode\n");
    gui_quit();
    return exited ? exit_code : rc;
}

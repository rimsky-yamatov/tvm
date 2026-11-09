/* gui_sdl.c - SDL2 backend: multiple windows, 8-bit paletted framebuffers. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"

typedef struct { SDL_Window *win; SDL_Renderer *ren; SDL_Texture *tex;
                 uint8_t *fb; uint32_t *px; int w, h; } Win;
static Win W[GUI_MAXWIN];
static uint32_t pal[256];
static int ready, scale = 2;

/* 5x7 font, ASCII 32..126, one byte per column, bit0 = top row */
static const uint8_t font[95][5] = {
 {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
 {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
 {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
 {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
 {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
 {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
 {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
 {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},
 {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
 {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
 {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
 {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
 {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
 {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
 {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
 {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
 {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
 {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x08,0x14,0x54,0x54,0x3C},
 {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x00,0x7F,0x10,0x28,0x44},
 {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
 {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
 {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
 {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
 {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08}
};

static void pal_init(void)
{
    static const uint32_t base[16] = {
        0x000000,0xAA0000,0x00AA00,0xAA5500,0x0000AA,0xAA00AA,0x00AAAA,0xAAAAAA,
        0x555555,0xFF5555,0x55FF55,0xFFFF55,0x5555FF,0xFF55FF,0x55FFFF,0xFFFFFF };
    int i, n, r, g, b;
    for (i = 0; i < 16; i++) pal[i] = 0xFF000000u | base[i];
    for (i = 16; i < 232; i++) {
        n = i - 16; r = n / 36; g = n / 6 % 6; b = n % 6;
        pal[i] = 0xFF000000u | (uint32_t)(((r ? 55 + 40*r : 0) << 16) | ((g ? 55 + 40*g : 0) << 8) | (b ? 55 + 40*b : 0));
    }
    for (i = 232; i < 256; i++) { n = 8 + (i - 232) * 10; pal[i] = 0xFF000000u | (uint32_t)((n << 16) | (n << 8) | n); }
}

static int init(void)
{
    const char *s;
    if (ready) return 1;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 0; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if ((s = getenv("TVM_SCALE")) && atoi(s) > 0 && atoi(s) <= 16) scale = atoi(s);
    pal_init();
    ready = 1;
    return 1;
}

static Win *G(int id) { return (id >= 0 && id < GUI_MAXWIN && W[id].fb) ? &W[id] : NULL; }

int gui_open(int w, int h)
{
    int i; Win *o;
    if (!init() || w < 1 || h < 1 || w > 1024 || h > 1024) return -1;
    for (i = 0; i < GUI_MAXWIN && W[i].fb; i++) ;
    if (i == GUI_MAXWIN) return -1;
    o = &W[i];
    o->w = w; o->h = h;
    o->win = SDL_CreateWindow("tvm", 40 + i * (w * scale + 16), 60, w * scale, h * scale, 0);
    if (o->win) o->ren = SDL_CreateRenderer(o->win, -1, 0);
    if (o->ren) o->tex = SDL_CreateTexture(o->ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    o->fb = calloc((size_t)w * h, 1);
    o->px = calloc((size_t)w * h, 4);
    if (!o->tex || !o->fb || !o->px) { gui_close(i); return -1; }
    return i;
}

void gui_close(int id)
{
    Win *o;
    if (id < 0 || id >= GUI_MAXWIN) return;
    o = &W[id];
    if (o->tex) SDL_DestroyTexture(o->tex);
    if (o->ren) SDL_DestroyRenderer(o->ren);
    if (o->win) SDL_DestroyWindow(o->win);
    free(o->fb); free(o->px);
    memset(o, 0, sizeof *o);
}

void gui_title(int id, const char *s) { Win *o = G(id); if (o) SDL_SetWindowTitle(o->win, s); }
void gui_clear(int id, int c) { Win *o = G(id); if (o) memset(o->fb, c, (size_t)o->w * o->h); }

void gui_pixel(int id, int x, int y, int c)
{
    Win *o = G(id);
    if (o && x >= 0 && y >= 0 && x < o->w && y < o->h) o->fb[y * o->w + x] = (uint8_t)c;
}

void gui_rect(int id, int x, int y, int w, int h, int c)
{
    Win *o = G(id); int x1, y1;
    if (!o || w <= 0 || h <= 0) return;
    x1 = x + w; y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > o->w) x1 = o->w;
    if (y1 > o->h) y1 = o->h;
    for (; y < y1 && x < x1; y++) memset(o->fb + y * o->w + x, c, (size_t)(x1 - x));
}

void gui_line(int id, int x0, int y0, int x1, int y1, int c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy, e2;
    for (;;) {
        gui_pixel(id, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

int gui_getpixel(int id, int x, int y)
{
    Win *o = G(id);
    return (o && x >= 0 && y >= 0 && x < o->w && y < o->h) ? o->fb[y * o->w + x] : -1;
}

void gui_recto(int id, int x, int y, int w, int h, int c)
{
    if (w <= 0 || h <= 0) return;
    gui_rect(id, x, y, w, 1, c);
    gui_rect(id, x, y + h - 1, w, 1, c);
    gui_rect(id, x, y, 1, h, c);
    gui_rect(id, x + w - 1, y, 1, h, c);
}

void gui_circle(int id, int cx, int cy, int r, int c)
{
    int x = r, y = 0, e = 1 - r;
    if (r < 0) return;
    while (x >= y) {
        gui_pixel(id, cx + x, cy + y, c); gui_pixel(id, cx - x, cy + y, c);
        gui_pixel(id, cx + x, cy - y, c); gui_pixel(id, cx - x, cy - y, c);
        gui_pixel(id, cx + y, cy + x, c); gui_pixel(id, cx - y, cy + x, c);
        gui_pixel(id, cx + y, cy - x, c); gui_pixel(id, cx - y, cy - x, c);
        y++;
        if (e < 0) e += 2 * y + 1; else { x--; e += 2 * (y - x) + 1; }
    }
}

void gui_fcircle(int id, int cx, int cy, int r, int c)
{
    int dy, dx;
    if (r < 0) return;
    if (r > 2048) r = 2048;
    for (dy = -r; dy <= r; dy++) {
        for (dx = 0; dx * dx + dy * dy <= r * r; dx++) ;
        gui_rect(id, cx - (dx - 1), cy + dy, 2 * dx - 1, 1, c);
    }
}

void gui_text(int id, int x, int y, const char *s, int c)
{
    int x0 = x, col, row;
    for (; *s; s++) {
        if (*s == '\n') { x = x0; y += 8; continue; }
        if (*s >= 32 && *s <= 126)
            for (col = 0; col < 5; col++)
                for (row = 0; row < 7; row++)
                    if (font[*s - 32][col] >> row & 1) gui_pixel(id, x + col, y + row, c);
        x += 6;
    }
}

void gui_pal(int i, uint32_t rgb) { pal[i & 255] = 0xFF000000u | (rgb & 0xFFFFFF); }

static void dump(int id, Win *o)   /* TVM_DUMP=prefix -> prefix<id>.ppm on every flip (headless testing) */
{
    const char *p = getenv("TVM_DUMP"); char name[256]; FILE *f; int i; uint32_t c;
    if (!p) return;
    snprintf(name, sizeof name, "%s%d.ppm", p, id);
    if (!(f = fopen(name, "wb"))) return;
    fprintf(f, "P6\n%d %d\n255\n", o->w, o->h);
    for (i = 0; i < o->w * o->h; i++) { c = pal[o->fb[i]]; fputc(c >> 16, f); fputc(c >> 8, f); fputc(c, f); }
    fclose(f);
}

void gui_flip(int id)
{
    Win *o = G(id); int i, n;
    if (!o) return;
    n = o->w * o->h;
    for (i = 0; i < n; i++) o->px[i] = pal[o->fb[i]];
    SDL_UpdateTexture(o->tex, NULL, o->px, o->w * 4);
    SDL_RenderCopy(o->ren, o->tex, NULL, NULL);
    SDL_RenderPresent(o->ren);
    dump(id, o);
}

static int wid(Uint32 sid)
{
    int i;
    for (i = 0; i < GUI_MAXWIN; i++) if (W[i].win && SDL_GetWindowID(W[i].win) == sid) return i;
    return -1;
}

/* event types: 0 none, 1 quit, 2 key(a=code,b=down), 3 move(a=x,b=y), 4 mouse down, 5 mouse up, 6 window close */
int gui_poll(int *win, int *type, int *a, int *b)
{
    SDL_Event e;
    *win = -1; *type = 0; *a = *b = 0;
    if (!ready) return 0;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: *type = 1; return 1;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_CLOSE) { *win = wid(e.window.windowID); *type = 6; return 1; }
            break;
        case SDL_KEYDOWN: case SDL_KEYUP:
            if (e.key.repeat) break;
            *win = wid(e.key.windowID); *type = 2;
            *a = (int)e.key.keysym.sym < 128 ? (int)e.key.keysym.sym : 128 + (int)(e.key.keysym.scancode & 127);
            *b = e.type == SDL_KEYDOWN;
            return 1;
        case SDL_MOUSEMOTION:
            *win = wid(e.motion.windowID); *type = 3; *a = e.motion.x / scale; *b = e.motion.y / scale; return 1;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
            *win = wid(e.button.windowID); *type = e.type == SDL_MOUSEBUTTONDOWN ? 4 : 5;
            *a = e.button.x / scale; *b = e.button.y / scale; return 1;
        }
    }
    return 0;
}

uint32_t gui_ticks(void) { return ready ? SDL_GetTicks() : 0; }
void gui_sleep(int ms) { if (ms > 0) { if (ready) SDL_Delay((Uint32)ms); } }

void gui_quit(void)
{
    int i;
    if (!ready) return;
    for (i = 0; i < GUI_MAXWIN; i++) gui_close(i);
    SDL_Quit();
    ready = 0;
}

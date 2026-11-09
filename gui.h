/* gui.h - backend interface. Implement these to port the GUI (SDL2, Win32, X11, canvas...). */
#ifndef GUI_H
#define GUI_H
#include <stdint.h>
#define GUI_MAXWIN 8
int      gui_open(int w, int h);                 /* -> window id, or -1 */
void     gui_close(int id);
void     gui_title(int id, const char *s);
void     gui_clear(int id, int c);
void     gui_pixel(int id, int x, int y, int c);
void     gui_rect(int id, int x, int y, int w, int h, int c);
void     gui_line(int id, int x0, int y0, int x1, int y1, int c);
void     gui_text(int id, int x, int y, const char *s, int c);
void     gui_pal(int i, uint32_t rgb);
void     gui_flip(int id);
int      gui_poll(int *win, int *type, int *a, int *b);   /* 1 if an event was returned */
uint32_t gui_ticks(void);
void     gui_sleep(int ms);
void     gui_quit(void);
#endif

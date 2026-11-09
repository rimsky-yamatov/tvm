/* gui_events.c - replaces gui_poll() with a fixed event script so GUI programs can be tested headlessly.
 * Script for examples/win2.asm: mouse moves in window 1 and 0, one idle poll, then Esc. */
int gui_poll(int *win, int *type, int *a, int *b)
{
    static int n;
    *win = -1; *type = 0; *a = *b = 0;
    switch (n++) {
    case 0: *win = 1; *type = 3; *a = 50;  *b = 60; return 1;   /* move in window B */
    case 1: *win = 0; *type = 3; *a = 120; *b = 30; return 1;   /* move in window A */
    case 2: return 0;                                           /* idle             */
    case 3: *win = 0; *type = 2; *a = 27;  *b = 1;  return 1;   /* Esc down         */
    }
    return 0;
}

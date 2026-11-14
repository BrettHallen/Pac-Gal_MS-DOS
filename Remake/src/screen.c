#include "screen.h"
#include <stdio.h>

unsigned char scr_color_attr(int fg, int bg)
{
    unsigned char a = (unsigned char)(((bg & 7) << 4) | (fg & 15));
    if (fg & 16) a |= 0x80;
    return a;
}

void scr_init(Screen *s)
{
    s->attr = 0x07;
    s->show_cursor = 0;
    scr_cls(s);
}

void scr_cls(Screen *s)
{
    for (int r = 0; r < SCR_ROWS; r++)
        for (int c = 0; c < SCR_COLS; c++) {
            s->cell[r][c].ch = ' ';
            s->cell[r][c].attr = (unsigned char)(s->attr & 0x70) | 0x07;
        }
    s->row = 1;
    s->col = 1;
}

void scr_color(Screen *s, int fg, int bg) { s->attr = scr_color_attr(fg, bg); }

void scr_locate(Screen *s, int row, int col)
{
    if (row < 1) row = 1;
    if (row > SCR_ROWS) row = SCR_ROWS;
    if (col < 1) col = 1;
    if (col > SCR_COLS) col = SCR_COLS;
    s->row = row;
    s->col = col;
}

void scr_put_at(Screen *s, int row, int col, unsigned char ch, unsigned char attr)
{
    if (row < 1 || row > SCR_ROWS || col < 1 || col > SCR_COLS) return;
    s->cell[row - 1][col - 1].ch = ch;
    s->cell[row - 1][col - 1].attr = attr;
}

void scr_putc(Screen *s, unsigned char ch)
{
    scr_put_at(s, s->row, s->col, ch, s->attr);
    if (++s->col > SCR_COLS) {          /* wrap; never scrolls (the game never needs it) */
        s->col = 1;
        if (s->row < SCR_ROWS) s->row++;
    }
}

void scr_print(Screen *s, const char *cp437)
{
    while (*cp437) scr_putc(s, (unsigned char)*cp437++);
}

void scr_print_int(Screen *s, int n)
{
    char buf[16];
    snprintf(buf, sizeof buf, n < 0 ? "%d " : " %d ", n);
    scr_print(s, buf);
}

int scr_char(const Screen *s, int row, int col)
{
    if (row < 1 || row > SCR_ROWS || col < 1 || col > SCR_COLS) return 219;
    return s->cell[row - 1][col - 1].ch;
}

int scr_attr(const Screen *s, int row, int col)
{
    if (row < 1 || row > SCR_ROWS || col < 1 || col > SCR_COLS) return 7;
    return s->cell[row - 1][col - 1].attr;
}

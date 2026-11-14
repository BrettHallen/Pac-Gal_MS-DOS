/* screen.h - an 80x25 CGA-style text screen with BASIC-like PRINT/LOCATE/COLOR/SCREEN.
 *
 * PAC-GAL uses the screen itself as its map (it reads characters and colours
 * back with SCREEN()), so the recreation keeps a real character/attribute
 * buffer, exactly like video memory at B800:0000.
 */
#ifndef PACGAL_SCREEN_H
#define PACGAL_SCREEN_H

#define SCR_COLS 80
#define SCR_ROWS 25

typedef struct {
    unsigned char ch;   /* CP437 code */
    unsigned char attr; /* bit7 blink, bits6-4 background, bits3-0 foreground */
} Cell;

typedef struct {
    Cell cell[SCR_ROWS][SCR_COLS];
    int row, col;          /* 1-based cursor, like LOCATE */
    unsigned char attr;    /* current COLOR */
    int show_cursor;       /* only while INPUT is waiting */
} Screen;

/* BASIC COLOR fg,bg -> attribute byte (fg 16..31 = blinking fg-16). */
unsigned char scr_color_attr(int fg, int bg);

void scr_init(Screen *s);
void scr_cls(Screen *s);
void scr_color(Screen *s, int fg, int bg);
void scr_locate(Screen *s, int row, int col);
void scr_putc(Screen *s, unsigned char ch);           /* PRINT CHR$(ch); */
void scr_print(Screen *s, const char *cp437);          /* PRINT "...";    */
void scr_print_int(Screen *s, int n);                  /* PRINT n;  (" 468 ") */
void scr_put_at(Screen *s, int row, int col, unsigned char ch, unsigned char attr);

/* SCREEN(row,col) and SCREEN(row,col,1).  Off-screen reads return a solid wall. */
int scr_char(const Screen *s, int row, int col);
int scr_attr(const Screen *s, int row, int col);

#endif

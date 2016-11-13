/* ncurses wrapper for bitbox simple text mode
 *
 * Copyright 2015, Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * This file is distributed under the terms of the MIT license.
 */

#include <simple.h>

int X;
int Y;

enum attributes{
	A_NORMAL,
	A_REVERSE,
	A_BOLD
};

#define LINES 30
#define COLUMNS 80

#define WHITE 0xFFFF
#define BLACK 0x0000
#define PINK  0xF00F
#define RED   0xF800
#define BLUE  0x001F
#define GREEN 0x07E0
#define YELLOW (GREEN | RED)
#define CYAN   (GREEN | BLUE)

static inline void initscr()
{
	text_color = 0;
	clear();

	palette[A_NORMAL]      = 0xFFFF0000; // boring white on black
	palette[A_REVERSE]     = 0xFF1F0000 | PINK; // light pink on pink
	palette[A_BOLD]        = 0xFF000000; // gold on black
	palette[3]             = 0xFFFF0000 | PINK;
	palette[4]             = 0xFFFF0000 | BLUE;
	palette[5]             = 0xFFFF0000 | GREEN;
	palette[6]             = 0xFFFF0000 | YELLOW;
	palette[A_NORMAL  | 8] = 0xFFFF00FF; // white on blue (cursor)
	palette[A_REVERSE | 8] = 0x000000FF; // black on blue (cursor)
	palette[A_BOLD    | 8] = 0xFF0000FF; // gold on blue (cursor)
}

static inline void erase()
{
	clear();
}

static inline void refresh()
{
	vram_attr[Y][X] |= 8;
}

static inline void mvaddstr(int y, int x, char* text)
{
	int k = print_at(x, y, text);

	Y = y;
	X = (x + k) % 80;
}

static inline void addstr(char* text)
{
	int k = print_at(X, Y, text);
	X = (X + k) % 80;
}

static inline void addch(char ch)
{
	vram[Y][X] = ch;
	vram_attr[Y][X] = text_color;
	X++;
	if (X > 79) X = 0;
}

static inline void move(int y, int x)
{
	X = x;
	Y = y;
}

static inline void attrset(int attr)
{
	text_color = attr;
};


static inline int getch()
{
	struct event e = event_get();
	if (e.type != evt_keyboard_press)
		return KEY_ERR;

	return e.kbd.sym;
}

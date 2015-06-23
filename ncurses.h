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

static const int LINES = 29;
static const int ERR = -1;

static inline void initscr()
{
	text_color = 0;
	clear();

	palette[A_NORMAL]      = 0xFFFF0000; // boring white on black
	palette[A_REVERSE]     = 0x0000FFFF; // boring black on white
	palette[A_BOLD]        = 0xFF000000;
	palette[A_NORMAL  | 8] = 0xFFFF7777; // boring white on grey
	palette[A_REVERSE | 8] = 0x00007777; // boring black on grey
	palette[A_BOLD    | 8] = 0xFF007777;
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
	print_at(x, y, text);

	Y = y;
	X = (x + strlen(text)) % 80;
}

static inline void addstr(char* text)
{
	print_at(X, Y, text);
	X = (X + strlen(text)) % 80;
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
		return ERR;

	return e.kbd.key;
}

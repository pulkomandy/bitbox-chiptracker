/* ncurses wrapper for bitbox text mode
 *
 * Copyright 2015, Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * This file is distributed under the terms of the MIT license.
 */

#include "lib/textmode/textmode.h"
#include "lib/events/events.h"

int X;
int Y;
extern uint8_t text_color;

enum attributes{
	A_NORMAL,
	A_REVERSE,
	A_BOLD
};

#define LINES 30
#define COLUMNS 80

#define WHITE  RGB(255,255,255)
#define BLACK  RGB(0,0,0)
#define PINK   RGB(200,0,200)
#define LIGHTPINK   RGB(255,130,255)
#define RED    RGB(255,0,0)
#define BLUE   RGB(0,0,255)
#define GREEN  RGB(0,255,0)
#define YELLOW RGB(255,255,0)
#define CYAN   RGB(0,255,255)

static inline void initscr()
{
	text_color = 0;
	clear();

    set_palette(A_NORMAL, WHITE,BLACK); // boring white on black
	set_palette(A_REVERSE, LIGHTPINK, PINK); // light pink on pink
	set_palette(A_BOLD, YELLOW, BLACK); // gold on black
	set_palette(3, WHITE, PINK);
	set_palette(4, WHITE, BLUE);
	set_palette(5, WHITE, GREEN);
	set_palette(6, WHITE, YELLOW);

	set_palette(A_NORMAL  | 8,WHITE, BLUE); // white on blue (cursor)
	set_palette(A_REVERSE | 8,BLACK, BLUE); // black on blue (cursor)
	set_palette(A_BOLD    | 8,YELLOW, BLUE); // gold on blue (cursor)
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
	int k = print_at(x, y, text_color, text);

	Y = y;
	X = (x + k) % 80;
}

static inline void addstr(char* text)
{
	int k = print_at(X, Y, text_color, text);
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

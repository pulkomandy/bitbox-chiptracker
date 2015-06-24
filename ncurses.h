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

static const int LINES = 39;

enum keycodes{
	ERR = -1,
	KEY_RIGHT = 128,
	KEY_LEFT = 129,
	KEY_DOWN = 130,
	KEY_UP = 131
};


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

static const int normal_table[] = {
	ERR,ERR,ERR,ERR,'a','b','c','d','e','f','g','h','i','j','k','l','m','n',
	'o','p','q','r','s','t','u','v','w','x','y','z','1','2','3','4','5','6',
	'7','8','9','0','\n',0x1B,8,'\t',' ','-','=','[',']','\\','#',';','\'','`',
	',','.','/',
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP,
	[224]=ERR,ERR,ERR,ERR,ERR,ERR,ERR,ERR
};

static const int shift_table[] = {
	ERR,ERR,ERR,ERR,'A','B','C','D','E','F','G','H','I','J','K','L','M','N',
	'O','P','Q','R','S','T','U','V','W','X','Y','Z','!','@','#','$','%','&',
	'^','*','(',')','\n',0x1B,8,'\t',' ','_','+','{','}','|','#',':','\\','"',
	'~','<','>','?',
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP,
	[224]=ERR,ERR,ERR,ERR,ERR,ERR,ERR,ERR
};

static const int ctrl_table[] = {
	ERR,ERR,ERR,ERR,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,26,'1','2','3','4','5','6',
	'7','8','9','0','\n',0x1B,8,'\t',' ','-','=','[',']','\\',ERR,';','\'','`',
	',','.','/',
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP,
	[224]=ERR,ERR,ERR,ERR,ERR,ERR,ERR,ERR
};

static inline int getch()
{
	struct event e = event_get();
	if (e.type != evt_keyboard_press)
		return ERR;

	// Unfortunately kbd_map is not complete enough, let's add some things...
	if (e.kbd.mod & (LCtrl|RCtrl))
		return ctrl_table[e.kbd.key];
	if (e.kbd.mod & (LShift|RShift))
		return shift_table[e.kbd.key];
	return normal_table[e.kbd.key];
}

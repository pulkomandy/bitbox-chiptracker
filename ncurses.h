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

/*
// already defined in bitbox.h
enum keycodes{
	KEY_ERR = -1,
	KEY_RIGHT = 128,
	KEY_LEFT = 129,
	KEY_DOWN = 130,
	KEY_UP = 131
};
*/

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


//TODO:  it would be nice to just re-use the data from bitbox/lib/evt_queue.c 
// so we don't have to hack it in here, too:
#ifdef KEYB_FR
static const int normal_table[83] = {
	KEY_ERR,KEY_ERR,KEY_ERR,KEY_ERR,'q','b','c','d','e','f','g','h','i','j','k','l',',','n',
	'o','p','a','r','s','t','u','v','z','x','y','w','&',0xe9,'\"','\'','(','-',
	0xe8,'_',0xe7,0xe0,'\n',0x1B,'\b','\t',' ',')','=','^','$','\\','#','m',0xF9,'*',
	';',':','!',
	[76]=127, // delete
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP
};
static const int shift_table[83] = {
	KEY_ERR,KEY_ERR,KEY_ERR,KEY_ERR,'Q','B','C','D','E','F','G','H','I','J','K','L','?','N',
	'O','P','A','R','S','T','U','V','Z','X','Y','W','1','2','3','4','5','6',
	'7','8','9','0','\n',0x1B,'\b','\t',' ',0xB0,'+',0xa8,0xa3,'\\','#','M','%',0xB5,
	'.','/',0xA7,
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP,
};
static const int ctrl_table[83] = {
	KEY_ERR,KEY_ERR,KEY_ERR,KEY_ERR, 17, 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10, 11, 12,',', 14,
	 15, 16, 1 , 18, 19, 20, 21, 22, 26, 24, 25, 23,'1','2','3','4','5','6',
	'7','8','9','0','\n',0x1B,'\b','\t',' ','-','=','[',']','\\',KEY_ERR,';','\'','`',
	 13,'.','/',
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP,
}; 
#else
static const int normal_table[83] = {
	KEY_ERR,KEY_ERR,KEY_ERR,KEY_ERR,'a','b','c','d','e','f','g','h','i','j','k','l','m','n',
	'o','p','q','r','s','t','u','v','w','x','y','z','1','2','3','4','5','6',
	'7','8','9','0','\n',0x1B,'\b','\t',' ','-','=','[',']','\\','#',';','\'','`',
	',','.','/',
	[76]=127, // delete
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP
};
static const int shift_table[83] = {
	KEY_ERR,KEY_ERR,KEY_ERR,KEY_ERR,'A','B','C','D','E','F','G','H','I','J','K','L','M','N',
	'O','P','Q','R','S','T','U','V','W','X','Y','Z','!','@','#','$','%','^',
	'&','*','(',')','\n',0x1B,'\b','\t',' ','_','+','{','}','|','#',':','"','~',
	'<','>','?',
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP,
};
static const int ctrl_table[83] = {
	KEY_ERR,KEY_ERR,KEY_ERR,KEY_ERR, 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,'1','2','3','4','5','6',
	'7','8','9','0','\n',0x1B,'\b','\t',' ','-','=','[',']','\\',KEY_ERR,';','\'','`',
	',','.','/',
	[79]=KEY_RIGHT,KEY_LEFT,KEY_DOWN,KEY_UP,
};
#endif


static inline int getch()
{
	struct event e = event_get();
	if (e.type != evt_keyboard_press || e.kbd.key >= 83 )
		return KEY_ERR;
	//message("e.kbd.key = %d -> %d\n", (int)e.kbd.key, normal_table[e.kbd.key]);
	// Unfortunately kbd_map is not complete enough, let's add some things...
	if (e.kbd.mod & (LCtrl|RCtrl))
		return ctrl_table[e.kbd.key];
	if (e.kbd.mod & (LShift|RShift))
		return shift_table[e.kbd.key];
	return normal_table[e.kbd.key];
}

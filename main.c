#include <stdint.h>

#include "stuff.h"

void game_init()
{
	initio();
	initchip();
	initgui();
	clear_song();
//	loadfile("bomberin.chp");
}

#ifndef EMULATOR
// This is needed for use of sprintf
void* __attribute__((used)) _sbrk(intptr_t increment)
{
	extern void* end;
	static void* ptr = &end;

	ptr += increment;
	return ptr;
}

void _exit()
{
	for(;;);
}

// I'm not sure why this is needed. Whatever.
void _read()
{
}

void _write()
{
}

void _close()
{
}

void _lseek()
{
}
#endif

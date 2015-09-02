#include <stdint.h>

#include "stuff.h"

void game_init()
{
	initio();
	initchip();
	initgui();
//	loadfile("bomberin.chp");
}

#ifndef EMULATOR
// This is needed for use of sprintf
void* _sbrk(intptr_t increment)
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
#endif

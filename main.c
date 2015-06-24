#include <stdint.h>

#include "stuff.h"

void game_init()
{
	initchip();
	initgui();
	loadfile("bomberintro.song");
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

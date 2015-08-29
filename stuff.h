#define TRACKLEN 32

#include <stdint.h>

#define NINST 64

typedef uint8_t u8;
typedef uint16_t u16;
typedef int8_t s8;
typedef int16_t s16;
typedef uint32_t u32;

struct trackline {
	u8	note;
	u8	instr;
	u8	cmd[2];
	u8	param[2];
};

struct track {
	struct trackline	line[256];
};

struct instrline {
	u8			cmd;
	u8			param;
};

struct instrument {
	int			length;
	struct instrline	line[256];
};

struct songline {
	u8			track[4];
	u8			transp[4];
};


void initchip();
void playroutine();

void readsong(int pos, int ch, u8 *dest);
void readtrack(int num, int pos, struct trackline *tl);
void readinstr(int num, int pos, u8 *il);

void silence();
void iedplonk(int, int);

void initgui();
void guiloop();
int packcmd(u8 ch);

void startplaysong(int);
void startplaytrack(int);

// input/output stuff
void loadfile(char *);
void savefile(char *);
void clear_song();

extern u8 trackpos;
extern u8 playtrack;
extern u8 playsong;
extern u16 songpos;
extern u16 songlen;
extern u8 songspeed;
extern int tracklen;

extern struct songline song[256];
extern struct track track[32];
extern struct instrument instrument[NINST];

extern char filename[32];
extern char alert[64];

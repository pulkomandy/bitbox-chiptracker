#define TRACKLEN 32

#include <stdint.h>

#define NINST 64
#define NINSTLINE 50  // number of lines to make an instrument
#define NTRACK 64
#define NTRACKLINE 16382

void message (const char *fmt, ...);

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

/*
struct track {
	struct trackline	line[256];
};
*/

struct instrline {
	u8			cmd;
	u8			param;
};

struct instrument {
	int			length;
	struct instrline	line[NINSTLINE];
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
void initio();
void loadfile(char *);
void savefile(char *);
void clear_song();

extern u8 trackpos;
extern u8 playtrack;
extern u8 playsong;
extern u16 songpos;
extern u16 songlen;
extern u16 numtracks;
extern u8 songspeed;
extern int tracklen;

extern struct songline song[256];
//extern struct track track[NTRACK];
extern struct trackline tracking[NTRACKLINE];
extern struct instrument instrument[NINST];
inline struct trackline *track(int track_index, int track_pos)
{
    return &tracking[(track_index-1)*tracklen + track_pos];
}
int realign_tracks(int new_tracklen);

extern char filename[32];
extern char alert[64];

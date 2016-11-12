#define TRACKLEN 32

#include <stdbool.h>
#include <stdint.h>

#define NINST 40
#define NINSTLINE 48  // number of lines to make an instrument
//#define NTRACK 64
//#define NTRACKLINE 12288
#define NTRACKLINE 8192
// note that  8192 = 32*256 is the total number of tracklines you'd expect 
// with the old constraints (32 notes/track, 256 tracks total)

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

void readsong(int pos, uint8_t ch, u8 *dest);
void readtrack(uint8_t num, uint8_t pos, struct trackline *tl);
void readinstr(uint8_t num, uint8_t pos, u8 *il);

void silence();
void iedplonk(int, int);

void initgui();
void guiloop();
int packcmd(u8 ch);

void startplaysong(int);
void startplaytrack(int);

// input/output stuff
void initio();
bool loadfile(char *);
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
inline struct trackline *track(uint8_t track_index, uint8_t track_pos)
{
    return &tracking[(track_index-1)*tracklen + track_pos];
}
int realign_tracks(int new_tracklen);

extern char filename[13]; // FatFS short filename constraints:  8.3 + null char


extern char alert[32];
void setalert(const char *alerto);

void redrawgui();
int rsscanf(const char* str, const char* format, ...);

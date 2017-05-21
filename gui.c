#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ncurses.h"
#include <math.h>

#include "lib/events/events.h"

#include "lib/chiptune/chiptune.h"
#include "fatfs/ff.h"

// TODO:  
// FEATURES:
// * ` on a track, if on a note, goes to edit that note's instrument
// * Home/End - sends you to tracky songy = 0 type of thing, depending on tab
// * PgUp/PgDn - sends you up/down the list by 16 or so lines
// * shift+space -> play song from beginning
// * ctrl+space -> loop song?
// * shift+tab -> move backwards through tabs
// ALGORITHMS
// * make numtracks go up when adding a note to a track, not just when moving to a new one
// * efficiently clear out instrument column when changing instrument when next one has less length


#include "stuff.h"

// choose the way to write your scale here:
#ifdef GERMAN_KEYS
static const char * const notenames[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "H-"};
#else
static const char * const notenames[] = {"C-", "C#", "D-", "Eb", "E-", "F-", "F#", "G-", "Ab", "A-", "Bb", "B-"};
#endif
uint8_t text_color;

static const char *keymap[2] = { // how to layout two octaves chromatically on the keyboard
	"zsxdcvgbhnjm,l.;/", // first octave
	"q2w3er5t6y7ui9o0p" // second octave
};

static const char *validcmds = "0bdfijlmtvw~+=";

#define SETLO(v,x) v = ((v) & 0xf0) | (x)
#define SETHI(v,x) v = ((v) & 0x0f) | ((x) << 4)
int songx, songy, songoffs;
u16 songlen=1;
int trackx=0, tracky=0, trackoffs=0;
int instrx=0, instry=0, instroffs=0;
int currtrack=1, currinstr=1;
int currtab=0;
int octave=4;

char cmd[16] = {0, 0}; // we do this to avoid undefined behavior in setcmd
char cmdinput[8];
u8 cmdinputpos;
u8 cmdinputnumeric;

enum {
	MODE_NONE = 0,
	MODE_EDIT = 1
};
int mode = MODE_NONE;

struct instrument iclip;
struct trackline tclip[256];

static void drawgui();

static void resetgui()
{
	mode = MODE_NONE;
	currtab = 0;
	songx = 0;
	songy = 0;
	currtrack = 1;
	tracky = 0;
	trackx = 0;
	currinstr = 1;
	instry = 0;
	instrx = 0;
}

void setalert(const char *alerto)
{
	int i = print_at(0, LINES - 1, text_color, alerto);
	char* alert = &vram[LINES - 1][0];
	for(; i < 32; i++)
		alert[i] = ' ';
}

static void setcmd(const char *cmdo)
{
	if(!cmd[0]) // if cmd had nothing in it
	{
		if (cmd[1] != cmdo[1]) // check if the previous command was the desired command
		{	// if not, then we need to remove previous input.
			cmdinput[0] = 0; // kill previous input
			cmdinputpos = 0;
		}
		strcpy(cmd, cmdo);
	}
	else if (cmd[0] != cmdo[0]) // command did not start with cmdo[0]
	{
		strcpy(cmd, cmdo);
		cmdinput[0] = 0;	// zero the input string, we just changed input.
		cmdinputpos = 0;
	}
	else // cmd[0] was cmdo[0], so we want to remove command.
	{
		cmd[0] = 0;
		return;
	}
	switch (cmd[0])
	{
	case 'b': // base filename
	case 'f': // file to open
		cmdinputnumeric = 0;
		break;
	case 't': // tracklength
	case 's': // songspeed 
		cmdinputnumeric = 1;
		break;
	}
}

void evalcmd()
{
	if (cmdinput[0])
	{
		int result;
		switch (cmd[0])
		{
		case 'b': // basename of file
			sprintf(filename, "%s.song", cmdinput);
			break;
		case 'f': // file to open
			silence();
			char newfile[13];
			sprintf(newfile, "%s.song", cmdinput);
			if (loadfile(newfile)) {
				cmd[0] = 0;
				resetgui();
				redrawgui();
			}
			break;
		case 't': // tracklength
			if (1 == sscanf(cmdinput, "%d", &result))
			{
				if (result > 0 && result < 257)
				{
					if (realign_tracks(result))
					{
						setalert("lost some higher tracks due to memory constraints!");
						if (currtrack*tracklen > NTRACKLINE)
							currtrack = NTRACKLINE/tracklen;
					}
					drawgui();
				}
				else
					setalert("need tracklength > 0 and < 257");
			}
			else
				setalert("unknown input!!");
			break;
		case 's': // songspeed 
			if (1 == sscanf(cmdinput, "%d", &result))
			{
				if (result > 0 && result < 256)
					songspeed = result;
				else
					setalert("need songspeed > 0 and < 256");
			}
			else
				setalert("unknown input!!");
			break;
		}
	}
	cmd[0] = 0;
	cmdinput[0] = 0;
	cmdinputpos = 0;
}


static void listfiles(void)
{
	DIR dir;
	FRESULT res;
	FILINFO fno;

	res = f_opendir(&dir, "");
	if (res != FR_OK)
		return;

	clear();

	res = f_readdir(&dir, &fno);
	int line = 6;
	for (;;)
	{
		res = f_readdir(&dir, &fno);

		// End of dir?
		if (res != FR_OK || fno.fname[0] == 0)
			break;

		char* pos = strchr(fno.fname, '.');
		if (pos == NULL) {
			continue;
		}

		if (!strncmp(".SON", pos, 4) || !strncmp(".son", pos, 4))
		{
			print_at(2, line++, text_color, fno.fname);
		}
	}

	print_at(2, line++, text_color, "-------------");
	f_closedir(&dir);
}


int hexdigit(char c) {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

int freqkey(int c) {
	char *s;
	int f = -1;

	if(c == '-' /*|| c == KEY_DC*/) return 0;
	if(c == '=') return 12 * 9 + 2; // Stop oscillator (in instruments)
	if(c > 0 && c < 256) {
		s = strchr(keymap[0], c);
		if(s) {
			f = (s - (keymap[0])) + octave * 12 + 1;
		} else {
			s = strchr(keymap[1], c);
			if(s) {
				f = (s - (keymap[1])) + octave * 12 + 12 + 1;
			}
		}
	}
	if(f > 12 * 9 + 1) return -1;
	return f;
}

//void exitgui() {
//	endwin();
//}

void initgui() {
	initscr();
	//noecho();
	//keypad(stdscr, TRUE);
	//raw();
	redrawgui();
	//atexit(exitgui);
}

static void hexstr(char* target, uint8_t value)
{
	static const char hex[] = "0123456789ABCDEF";
	target[0] = hex[value >> 4];
	target[1] = hex[value & 0xF];
}

static void scrollbar(int x, int y, int height, int pos, int range)
{
	int start = pos * height / range;
	int end = start + height * height / range;

	for (int i = 0; i < height; i++)
	{
		uint8_t* tgt = &vram_attr[y+i][x];
		if (i >= start && i <= end)
			*tgt = 6;
		else
			*tgt = 0;
	}
}

static void drawsonged(int x, int y, int height) {
	int i, j, k = 0;
	char buf[6];

	int showline;

	if (playsong)
		showline = (songpos - 1 + songlen) % songlen;
	else
		showline = songy;

	if(showline < songoffs) songoffs = showline;
	if(showline >= songoffs + height) songoffs = showline - height + 1;

	buf[5] = 0;
	buf[2] = ':';

	for(i = 0; i < songlen; i++) {
		if(i >= songoffs && i - songoffs < height) {
			move(y + k, x + 0);
			if(i == songy)
				attrset(A_BOLD);
			else
				attrset(A_NORMAL);
			hexstr(buf, i);
			buf[3] = 0;
			addstr(buf);
			for(j = 0; j < 4; j++) {
				addch(' ');
				hexstr(buf, song[i].track[j]);
				hexstr(buf + 3, song[i].transp[j]);
				addstr(buf);
			}
			if(playsong && songpos == (i + 1))
				*(uint16_t*)(&vram_attr[y+k][x]) = 0x0505;
			k++;
		}
	}

	// blank lines at song end
	while (k < height) {
		move(y + k, x + 0);
		addstr("                           ");
		k++;
	}

	if (songlen > height) {
		scrollbar(x-1, y, height, songoffs, songlen);
	}
}

static void drawtracked(int x, int y, int height) {
	int i, j;
	char buf[6];

	if (height > tracklen)
		height = tracklen;

	int position;
	if (playtrack)
		position = (trackpos - 1 + tracklen) % tracklen ;
	else
		position = tracky;

	if(position < trackoffs) trackoffs = position;
	if(position >= trackoffs + height) trackoffs = position - height + 1;

	for(i = 0; i < tracklen; i++) {
		if(i >= trackoffs && i - trackoffs < height) {
			move(y + i - trackoffs, x + 0);
			if(i == tracky)
				attrset(A_BOLD);
			else
				attrset(A_NORMAL);
			buf[2] = ':';
			buf[3] = ' ';
			buf[4] = 0;
			hexstr(buf, i);
			addstr(buf);
			if(track(currtrack,i)->note) {
				sprintf(buf, "%s%d ",
					notenames[(track(currtrack,i)->note - 1) % 12],
					(track(currtrack,i)->note - 1) / 12);
			} else {
				buf[0] = buf[1] = buf[2] = '-';
			}
			addstr(buf);

			buf[2] = 0;
			hexstr(buf, track(currtrack, i)->instr);
			addstr(buf);
			for(j = 0; j < 2; j++) {
				buf[0] = ' ';
				if(track(currtrack,i)->cmd[j]) {
					buf[1] = track(currtrack,i)->cmd[j];
					hexstr(buf + 2, track(currtrack,i)->param[j]);
				} else {
					buf[1] = buf[2] = buf[3] = '.';
					buf[4] = 0;
				}
				addstr(buf);
			}
			if(playtrack && ((i + 1) % tracklen) == trackpos)
				*(uint16_t*)(&vram_attr[y+i-trackoffs][x]) = 0x0505;
		}
	}

	if (tracklen > height) {
		scrollbar(x-1, y, height, trackoffs, tracklen);
	}
}

static void drawinstred(int x, int y, int height) {
	int i, j = 0;
	char buf[8];

	// TODO:
	// what is modifying instry/instroffs to be -1?
	// this should not be required!
	if(instroffs <0) instroffs=0;
	if(instry <0) instry=0;
	// but for some reason somehow these guys get turned to -1 somewhere

	if(instry >= instrument[currinstr].length) instry = instrument[currinstr].length - 1;

	if(instry < instroffs) instroffs = instry;
	if(instry >= instroffs + height) instroffs = instry - height + 1;

	for(i = 0; i < instrument[currinstr].length; i++) {
		if(i >= instroffs && i - instroffs < height) {
			move(y + j, x + 0);
			if(i == instry) attrset(A_BOLD);
			sprintf(buf, "%02x: %c ", i, instrument[currinstr].line[i].cmd);
			addstr(buf);
			if(instrument[currinstr].line[i].cmd == '+' || instrument[currinstr].line[i].cmd == '=') {
				if(instrument[currinstr].line[i].param > 83)
					sprintf(buf, "---");
				else if(instrument[currinstr].line[i].param) {
					sprintf(buf, "%s%d",
						notenames[(instrument[currinstr].line[i].param - 1) % 12],
						(instrument[currinstr].line[i].param - 1) / 12);
				} else {
					buf[0] = buf[1] = buf[2] = '-';
					buf[3] = 0;
				}
			} else {
				hexstr(buf, instrument[currinstr].line[i].param);
				buf[2] = '-';
				buf[3] = 0;
			}
			addstr(buf);

			sprintf(buf, " %c ", instrument[currinstr].line[i].cmd2);
			addstr(buf);

			if(instrument[currinstr].line[i].cmd2 == '+' || instrument[currinstr].line[i].cmd2 == '=') {
				if(instrument[currinstr].line[i].param2 > 83)
					sprintf(buf, "---");
				else if(instrument[currinstr].line[i].param2) {
					sprintf(buf, "%s%d",
						notenames[(instrument[currinstr].line[i].param2 - 1) % 12],
						(instrument[currinstr].line[i].param2 - 1) / 12);
				} else {
					buf[0] = buf[1] = buf[2] = '-';
					buf[3] = 0;
				}
			} else {
				hexstr(buf, instrument[currinstr].line[i].param2);
				buf[2] = '-';
				buf[3] = 0;
			}
			addstr(buf);

			attrset(A_NORMAL);
			j++;
		}
	}

	while (j < height)
	{
		move(y + j, x + 0);
		addstr("               ");
		j++;
	}
}

void handleinput() {
int c, x;
if ((c = getch()) != KEY_ERR)
{
	if (cmd[0])
	{
		//	 numbers			  uppercase letters   lowercase letters		_ or -
		if ( (c>=48 && c<=57) || (c>=65 && c<=90) || (c>=97 && c<=122) || (c == 95 || c == 45) )
		{
			if (cmdinputnumeric)
			{
				// only allow inputting numbers
				if (c>=48 && c<=57)
				{
					if (cmdinputpos < 2)
					{
						cmdinput[cmdinputpos] = c;
						cmdinput[++cmdinputpos] = 0;
					}
					else
					{
						cmdinput[cmdinputpos] = c;
						cmdinput[cmdinputpos+1] = 0;
					}
				}
			}
			else
			{
				if (cmdinputpos < 7)
				{
					cmdinput[cmdinputpos] = c;
					cmdinput[++cmdinputpos] = 0;
				}
				else
					cmdinput[cmdinputpos] = c;
			}
		}
		else switch (c)
		{
		case 8: // backspace
			if (cmdinputpos > 0)
			{
				if (cmdinputnumeric)
				{
					if (cmdinputpos == 2 && cmdinput[cmdinputpos])
						cmdinput[cmdinputpos] = 0;
					else
						cmdinput[--cmdinputpos] = 0;
				}
				else
				{
					if (cmdinputpos == 7 && cmdinput[cmdinputpos])
						cmdinput[cmdinputpos] = 0;
					else
						cmdinput[--cmdinputpos] = 0;
				}
			}
			break;
		case '\n':
			evalcmd();
			break;
		case 'T' - '@':
			setcmd("tracklength");
			break;
		case 'F' - '@':
			setcmd("base filename");
			break;
		case 'O' - '@':
			setcmd("file to open");
			listfiles();
			break;
		case 'S' - '@':
			setcmd("songspeed");
			break;
		}
	}
	else 
	switch(c) {
		case ' ':
			if (playsong || playtrack)
			{
				// stop play
				silence();
			}
			else
			{
				// start play
				if(currtab == 1) {
					startplaytrack(currtrack);
				} else if (currtab == 0) {
					startplaysong(songy);
				} else {
					startplaysong(0);
				}
			}
			break;
		case 10: // enter:  toggle edit mode
		case 13: // enter on other systems...
			if (mode & MODE_EDIT) 
			{
				mode -= MODE_EDIT; // lock it down
			}
			else
			{
				mode += MODE_EDIT; // allow editing
			}
			break;
		case 'N' - '@':
		case 'N':
		case 9:
			currtab++;
			if (mode & MODE_EDIT)
				currtab %= 3;
			else
				currtab %= 2; // this glosses over a bug
			break;
		case 'P' - '@':
		case 'P':
			currtab--;
			if (mode & MODE_EDIT)
				currtab %= 3;
			else
				currtab %= 2;
			break;
		case 'W' - '@':
			if (mode & MODE_EDIT)
				savefile(filename);
			break;
		case 'T' - '@':
			if (mode & MODE_EDIT)
				setcmd("tracklength");
			break;
		case 'F' - '@':
			setcmd("base filename");
			break;
		case 'O' - '@':
			setcmd("file to open");
			listfiles();
			break;
		case 'S' - '@':
			if (mode & MODE_EDIT)
				setcmd("songspeed");
			break;
		case '<':
			if(octave) octave--;
			break;
		case '>':
			if(octave < 8) octave++;
			break;
		case '[':
			if(currinstr > 1) {
				currinstr--;
				if (instrument[currinstr].length < instrument[currinstr+1].length)
					redrawgui();
			}
			break;
		case ']':
			if(currinstr < NINST-1) {
				currinstr++;
				if (instrument[currinstr].length < instrument[currinstr-1].length)
					redrawgui();
			}
			break;
		case '{':
			if(currtrack > 1) currtrack--;
			break;
		case '}':
			if((currtrack+1)*tracklen <= NTRACKLINE && currtrack < 255)  {
				currtrack++;
				// update the number of tracks -- TODO: probably should wait to put
				// something in them, but nevertheless keep track of it.
				if (numtracks < currtrack) 
					numtracks = currtrack;
			} else {
				setalert("can't fit more tracks in memory");
			}
			break;
		case '`':
			if(currtab == 0) {
				int t = song[songy].track[songx / 4];
				if(t) currtrack = t;
				currtab = 1;
			} else if(currtab == 1) {
				currtab = 0;
			}
			break;
//		case '#': // or '\\' ?
//			optimize();
//			break;
//		case '%': // or '=' ?
//			export();
//			break;
		case KEY_LEFT:
			switch(currtab) {
				case 0:
					if(songx) songx--;
					break;
				case 1:
					if(trackx) trackx--;
					break;
				case 2:
					if((mode&MODE_EDIT) && instrx) instrx--;
					break;
			}
			break;
		case KEY_RIGHT:
			switch(currtab) {
				case 0:
					if(songx < 15) songx++;
					break;
				case 1:
					if(trackx < 8) trackx++;
					break;
				case 2:
					if((mode&MODE_EDIT) && instrx < 5) instrx++;
					break;
			}
			break;
		case KEY_UP:
		case KEY_PAGEUP:
		{
			int off = (c == KEY_PAGEUP) ? 16 : 1;
			switch(currtab) {
				case 0:
					if(songy >= off) songy-=off;
					break;
				case 1:
					if(tracky >= off) {
						tracky-=off;
					} else {
						tracky = tracklen - 1;
					}
					break;
				case 2:
					if(instry >= off) instry-=off;
					break;
			}
			break;
		}
		case KEY_DOWN:
		case KEY_PAGEDOWN:
		{
			int off = (c == KEY_PAGEDOWN) ? 16 : 1;
			switch(currtab) {
				case 0:
					if(songy < songlen - off) songy+=off;
					break;
				case 1:
					if(tracky < tracklen - off) {
						tracky+=off;
					} else {
						tracky = 0;
					}
					break;
				case 2:
					if(instry < instrument[currinstr].length - off) instry+=off;
					break;
			}
			break;
		}
		case 'C': // copy
			if (mode & MODE_EDIT)
			{
				if(currtab == 2) {
					memcpy(&iclip, &instrument[currinstr], sizeof(struct instrument));
				} else if(currtab == 1) {
					memcpy(&tclip, &tracking[tracklen*currtrack], tracklen*sizeof(struct trackline));
				}
			}
			break;
		case 'V': // paste
			if (mode & MODE_EDIT)
			{
				if(currtab == 2) {
					memcpy(&instrument[currinstr], &iclip, sizeof(struct instrument));
				} else if(currtab == 1) {
					memcpy(&tracking[tracklen*currtrack], &tclip, tracklen*sizeof(struct trackline));
				}
			}
			break;
		case 8: // backspace
		case 127: // delete
			if(mode & MODE_EDIT) switch (currtab) {
				case 0:
					song[songy].track[songx/4] = 0;
					song[songy].transp[songx/4] = 0;
					break;
				case 1:
					memset(track(currtrack,tracky), 0, sizeof(struct trackline));
					break;
				case 2:
					instrument[currinstr].line[instry].cmd = '0';
					instrument[currinstr].line[instry].param = 0;
			}
			break;
		default:
			if(mode & MODE_EDIT) {
				x = hexdigit(c);
				// helpful for debugging:
				// sprintf(alert, "%d %x %c", (int) c, x, c);
				if(x >= 0) {
					if(currtab == 2
					&& instrx > 0
					&& instrument[currinstr].line[instry].cmd != '+'
					&& instrument[currinstr].line[instry].cmd != '=') {
						switch(instrx) {
							case 1: SETHI(instrument[currinstr].line[instry].param, x); break;
							case 2: SETLO(instrument[currinstr].line[instry].param, x); break;
							case 4: SETHI(instrument[currinstr].line[instry].param2, x); break;
							case 5: SETLO(instrument[currinstr].line[instry].param2, x); break;
						}
					}
					if(currtab == 1 && trackx > 0) {
						switch(trackx) {
							case 1: SETHI(track(currtrack,tracky)->instr, x); break;
							case 2: SETLO(track(currtrack,tracky)->instr, x); break;
							case 4: if(track(currtrack,tracky)->cmd[0])
								SETHI(track(currtrack,tracky)->param[0], x); break;
							case 5: if(track(currtrack,tracky)->cmd[0])
								SETLO(track(currtrack,tracky)->param[0], x); break;
							case 7: if(track(currtrack,tracky)->cmd[1])
								SETHI(track(currtrack,tracky)->param[1], x); break;
							case 8: if(track(currtrack,tracky)->cmd[1])
								SETLO(track(currtrack,tracky)->param[1], x); break;
						}
					}
					if(currtab == 0) {
						switch(songx & 3) {
							case 0: SETHI(song[songy].track[songx / 4], x); break;
							case 1: SETLO(song[songy].track[songx / 4], x); break;
							case 2: SETHI(song[songy].transp[songx / 4], x); break;
							case 3: SETLO(song[songy].transp[songx / 4], x); break;
						}
					}
				}
				x = freqkey(c);
				if(x >= 0) {
					if(currtab == 2
					&& instrx && instrx < 3
					&& (instrument[currinstr].line[instry].cmd == '+' || instrument[currinstr].line[instry].cmd == '=')) {
						instrument[currinstr].line[instry].param = x;
					}
					if(currtab == 2
					&& instrx > 3
					&& (instrument[currinstr].line[instry].cmd2 == '+' || instrument[currinstr].line[instry].cmd2 == '=')) {
						instrument[currinstr].line[instry].param2 = x;
					}
					if(currtab == 1 && !trackx) {
						track(currtrack,tracky)->note = x;
						if(x) {
							track(currtrack,tracky)->instr = currinstr;
						} else {
							track(currtrack,tracky)->instr = 0;
						}
						tracky++;
						tracky %= tracklen;
						if(x) iedplonk(x, currinstr);
					}
				}
				if(currtab == 2 && instrx == 0) {
					if(strchr(validcmds, c)) {
						instrument[currinstr].line[instry].cmd = c;
					}
				}
				if(currtab == 2 && instrx == 3) {
					if(strchr(validcmds, c)) {
						instrument[currinstr].line[instry].cmd2 = c;
					}
				}
				if(currtab == 1 && (trackx == 3 || trackx == 6 || trackx == 9)) {
					if(strchr(validcmds, c)) {
						if(c == '.' || c == '0') c = 0;
						track(currtrack,tracky)->cmd[(trackx - 3) / 3] = c;
					}
				}
				if(c == 'A') {
					if(currtab == 2) {
						struct instrument *in = &instrument[currinstr];

						if(in->length < NINSTLINE) {
							memmove(&in->line[instry + 2], &in->line[instry + 1], sizeof(struct instrline) * (in->length - instry - 1));
							instry++;
							in->length++;
							in->line[instry].cmd = '0';
							in->line[instry].param = 0;
						}
					} else if(currtab == 0) {
						if(songlen < 256) {
							memmove(&song[songy + 2], &song[songy + 1], sizeof(struct songline) * (songlen - songy - 1));
							songy++;
							songlen++;
							memset(&song[songy], 0, sizeof(struct songline));
						}
					}
				} else if(c == 'I') {
					if(currtab == 2) {
						struct instrument *in = &instrument[currinstr];

						if(in->length < NINSTLINE) {
							memmove(&in->line[instry + 1], &in->line[instry + 0], sizeof(struct instrline) * (in->length - instry));
							in->length++;
							in->line[instry].cmd = '0';
							in->line[instry].param = 0;
						}
					} else if(currtab == 0) {
						if(songlen < 256) {
							memmove(&song[songy + 1], &song[songy + 0], sizeof(struct songline) * (songlen - songy));
							songlen++;
							memset(&song[songy], 0, sizeof(struct songline));
						}
					}
				} else if(c == 'D') {
					if(currtab == 2) {
						struct instrument *in = &instrument[currinstr];

						if(in->length > 1) {
							memmove(&in->line[instry + 0], &in->line[instry + 1], sizeof(struct instrline) * (in->length - instry - 1));
							in->length--;
							if(instry >= in->length) instry = in->length - 1;
							redrawgui();
						}
					} else if(currtab == 0) {
						if(songlen > 1) {
							memmove(&song[songy + 0], &song[songy + 1], sizeof(struct songline) * (songlen - songy - 1));
							songlen--;
							if(songy >= songlen) songy = songlen - 1;
							redrawgui();
						}
					}
				}
			} 
			else { // 
				x = freqkey(c);

				if(x > 0) {
					iedplonk(x, currinstr);
				}
			}
			break;
	}
}
}

static void drawgui() {
	char buf[32];
	int songcols[] = {0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21, 22};
	int trackcols[] = {0, 4, 5, 7, 8, 9, 11, 12, 13};
	int instrcols[] = {0, 2, 3, 6, 8, 9};

	if (mode & MODE_EDIT)
	{
		mvaddstr(0, COLUMNS-22, "lock");
	}
	else
	{
		mvaddstr(0, COLUMNS-22, "edit");
	}
	if (playsong||playtrack)
	{
		mvaddstr(1, COLUMNS-22, "stop");
	}
	else
	{
		mvaddstr(1, COLUMNS-22, "play");
	}

	mvaddstr(2, 14, "                      ");
	mvaddstr(2, 14, filename);
	text_color = 3;
	if (mode & MODE_EDIT)
		text_color = 3;
	else
		text_color = A_REVERSE;
	sprintf(buf, "^S)ongspeed: %d  ", songspeed);
	mvaddstr(3, 1, buf);
	sprintf(buf, "^T)racklength: %d  ", tracklen);
	mvaddstr(3, 30, buf);
	mvaddstr(3, COLUMNS - 23, "^W)rite");

	text_color = 0;
	if (cmd[0] != 'f') {
		drawsonged(2, 6, LINES - 8);
	}

	buf[2] = 0;
	hexstr(buf, currtrack);
	mvaddstr(5, 37, buf);
	drawtracked(31, 6, LINES - 8);

	hexstr(buf, currinstr);
	mvaddstr(5, 58, buf);
	drawinstred(50, 6, LINES - 12);
	
	hexstr(buf, octave);
	mvaddstr(5, COLUMNS - 7, buf);

	// Draw channel volumes
	int p = 5;
	for (int o = 0; o < 4; o++)
	{
		int v = osc[o].volume >> 4;
		int k;
		for (k = 0; k < v; k++)
		{
			vram_attr[6 + k][p] = 3 + osc[o].waveform;
		}
		for (; k < 16; k++)
		{
			vram_attr[6 + k][p] = 0;
		}
		p += 6;
	}

	// Update cursor position
	int cx, cy;
	if (cmd[0])
	{
		int off = sprintf(buf, "new %s? %s", cmd, cmdinput);
		setalert(buf);
		cy = LINES - 1;
		cx = off;
	} else {
		switch(currtab) {
		case 0:
		default:
			cy = 6 + songy - songoffs;
			cx = 0 + 6 + songcols[songx];
			break;
		case 1:
			cy = 6 +tracky - trackoffs;
			cx = 29 + 6 + trackcols[trackx];
			break;
		case 2:
			cy = 6 + instry - instroffs;
			cx = 48 + 6 + instrcols[instrx];
			break;
		}

		if (cy < 6)
			return;
		if (cy >= LINES - 2)
			return;
	}

	move (cy, cx);
	refresh();
}

void redrawgui() {
	memset(vram_attr, 3, SCREEN_H * SCREEN_W);
	text_color = 3;
	mvaddstr(0, 0, " MUSIC CHIP TRACKER");

	mvaddstr(0, COLUMNS-29, "enter [    ]");
	mvaddstr(1, COLUMNS-29, "space [    ]");
	
	mvaddstr(2, 1, "^F)ilename:");
	
	mvaddstr(3, COLUMNS - 30, "^O)pen");
	
	mvaddstr(5, 1, "Song L1:\x1f\x1e R1:\x1f\x1e L2:\x1f\x1e R2:\x1f\x1e");

	mvaddstr(5, 31, "Track    {}");
	mvaddstr(5, 51, "Instr.    []");
	mvaddstr(5, COLUMNS - 15, "Octave:    <>");

	mvaddstr(7, COLUMNS - 15, "-- KEYS --");
	mvaddstr(8, COLUMNS - 15, "A)ppend");
	mvaddstr(9, COLUMNS - 15, "I)nsert");
	mvaddstr(10,COLUMNS - 15, "D)elete");
	mvaddstr(11,COLUMNS - 15, "C)opy");
	mvaddstr(12,COLUMNS - 15, "V) Paste");
	mvaddstr(13,COLUMNS - 15, "TAB switch");
	mvaddstr(14,COLUMNS - 15, "` Goto track");

	mvaddstr(15,COLUMNS - 15, "-- COMMANDS --");
	mvaddstr(16,COLUMNS - 15, "+ Rel. Note");
	mvaddstr(17,COLUMNS - 15, "= Abs. Note");
	mvaddstr(18,COLUMNS - 15, "b)itcrush");
	mvaddstr(19,COLUMNS - 15, "d)uty cycle");
	mvaddstr(20,COLUMNS - 15, "f)ade volume");
	mvaddstr(21,COLUMNS - 15, "i)nertia");
	mvaddstr(22,COLUMNS - 15, "j)ump");
	mvaddstr(23,COLUMNS - 15, "s(l)ide");
	mvaddstr(24,COLUMNS - 15, "pw(m)");
	mvaddstr(25,COLUMNS - 15, "t)ime delay");
	mvaddstr(26,COLUMNS - 15, "v)olume");
	mvaddstr(27,COLUMNS - 15, "w)aveform TSPN");
	mvaddstr(28,COLUMNS - 15, "~ Vibrato d/r");

	text_color = 0;
	drawgui();
}

void game_frame() {
	events_poll();
	// palette[A_NORMAL] = 0xFFFF0000 | PINK;
	playroutine();
	handleinput();
	drawgui();
	// palette[A_NORMAL]      = 0xFFFF0000; // boring white on black

}

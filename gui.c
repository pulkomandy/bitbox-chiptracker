#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ncurses.h"
#include <math.h>

// TODO:  
// * io bug??? when opening a file, it hangs somehow on the bitbox (emulator fine).
// FEATURES:
// * HELP bar (under octaves):  includes I, D, A, C, V in commands, chart of valid cmds
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

char *keymap[2] = { // how to layout two octaves chromatically on the keyboard
	"zsxdcvgbhnjm,l.;/", // first octave
	"q2w3er5t6y7ui9o0p" // second octave
};

char *validcmds = "0dfijlmtvw~+="; // TODO: need bitcrush

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

char alert[32];

enum {
	MODE_NONE = 0,
	MODE_EDIT = 1
};
int mode = MODE_NONE;

struct instrument iclip;
struct trackline tclip[256];

void resetgui()
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
	snprintf(alert, sizeof(alert), "%s                                ", alerto);
}

void setcmd(const char *cmdo)
{
	alert[0] = 0; // remove any alert
	if(!cmd[0]) // if cmd had nothing in it
	{
		snprintf(cmd, sizeof(cmd), "%s", cmdo);
		if (cmd[1] != cmdo[1]) // check if the previous command was the desired command
		{	// if not, then we need to remove previous input.
			cmdinput[0] = 0; // kill previous input
			cmdinputpos = 0;
		}
	}
	else if (cmd[0] != cmdo[0]) // command did not start with cmdo[0]
	{
		snprintf(cmd, sizeof(cmd), "%s", cmdo);
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
			snprintf(filename, sizeof(filename), "%s.chp", cmdinput);
			break;
		case 'f': // file to open
			silence();
			char newfile[13];
			snprintf(newfile, sizeof(newfile), "%s.chp", cmdinput);
			loadfile(newfile);
			resetgui();
			redrawgui();
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
					redrawgui();
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

int hexdigit(char c) {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

int freqkey(int c) {
	char *s;
	int f = -1;

	if(c == '-' /*|| c == KEY_DC*/) return 0;
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

void drawsonged(int x, int y, int height) {
	int i, j;
	char buf[6];

	if(songy < songoffs) songoffs = songy;
	if(songy >= songoffs + height) songoffs = songy - height + 1;

	for(i = 0; i < songlen; i++) {
		if(i >= songoffs && i - songoffs < height) {
			move(y + i - songoffs, x + 0);
			if(i == songy) attrset(A_BOLD);
			snprintf(buf, sizeof(buf), "%02x: ", i);
			addstr(buf);
			for(j = 0; j < 4; j++) {
				snprintf(buf, sizeof(buf), "%02x:%02x", song[i].track[j], song[i].transp[j]);
				addstr(buf);
				if(j != 3) addch(' ');
			}
			attrset(A_NORMAL);
			if(playsong && songpos == (i + 1)) addch('*');
			else addch(' ');
		}
	}

	// One blank line at song end
	move(y + i - songoffs, x + 0);
	addstr("---------------------------");
}

void drawtracked(int x, int y, int height) {
	int i, j;
	char buf[6];

	if(tracky < trackoffs) trackoffs = tracky;
	if(tracky >= trackoffs + height) trackoffs = tracky - height + 1;

	for(i = 0; i < tracklen; i++) {
		if(i >= trackoffs && i - trackoffs < height) {
			move(y + i - trackoffs, x + 0);
			if(i == tracky) attrset(A_BOLD);
			snprintf(buf, sizeof(buf), "%02x: ", i);
			addstr(buf);
			if(track(currtrack,i)->note) {
				snprintf(buf, sizeof(buf), "%s%d",
					notenames[(track(currtrack,i)->note - 1) % 12],
					(track(currtrack,i)->note - 1) / 12);
			} else {
				snprintf(buf, sizeof(buf), "---");
			}
			addstr(buf);
			snprintf(buf, sizeof(buf), " %02x", track(currtrack,i)->instr);
			addstr(buf);
			for(j = 0; j < 2; j++) {
				if(track(currtrack,i)->cmd[j]) {
					snprintf(buf, sizeof(buf), " %c%02x",
						track(currtrack,i)->cmd[j],
						track(currtrack,i)->param[j]);
				} else {
					snprintf(buf, sizeof(buf), " ...");
				}
				addstr(buf);
			}
			attrset(A_NORMAL);
			if(playtrack && ((i + 1) % tracklen) == trackpos) {
				addch('*');
			} else
				addch(' ');
		}
	}
}

void drawinstred(int x, int y, int height) {
	int i;
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
			move(y + i - instroffs, x + 0);
			if(i == instry) attrset(A_BOLD);
			snprintf(buf, sizeof(buf), "%02x: %c ", i, instrument[currinstr].line[i].cmd);
			addstr(buf);
			if(instrument[currinstr].line[i].cmd == '+' || instrument[currinstr].line[i].cmd == '=') {
				if(instrument[currinstr].line[i].param) {
					snprintf(buf, sizeof(buf), "%s%d",
						notenames[(instrument[currinstr].line[i].param - 1) % 12],
						(instrument[currinstr].line[i].param - 1) / 12);
				} else {
					snprintf(buf, sizeof(buf), "---");
				}
			} else {
				snprintf(buf, sizeof(buf), "%02x-", instrument[currinstr].line[i].param);
			}
			addstr(buf);
			attrset(A_NORMAL);
		}
	}
	move(y + i - instroffs, x + 0);
	addstr("--- - ---");
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
					if((mode&MODE_EDIT) && instrx < 2) instrx++;
					break;
			}
			break;
		case KEY_UP:
			switch(currtab) {
				case 0:
					if(songy) songy--;
					break;
				case 1:
					if(tracky) {
						tracky--;
					} else {
						tracky = tracklen - 1;
					}
					break;
				case 2:
					if(instry) instry--;
					break;
			}
			break;
		case KEY_DOWN:
			switch(currtab) {
				case 0:
					if(songy < songlen - 1) songy++;
					break;
				case 1:
					if(tracky < tracklen - 1) {
						tracky++;
					} else {
						tracky = 0;
					}
					break;
				case 2:
					if(instry < instrument[currinstr].length - 1) instry++;
					break;
			}
			break;
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
				// snprintf(alert, sizeof(alert), "%d %x %c", (int) c, x, c);
				if(x >= 0) {
					if(currtab == 2
					&& instrx > 0
					&& instrument[currinstr].line[instry].cmd != '+'
					&& instrument[currinstr].line[instry].cmd != '=') {
						switch(instrx) {
							case 1: SETHI(instrument[currinstr].line[instry].param, x); break;
							case 2: SETLO(instrument[currinstr].line[instry].param, x); break;
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
					&& instrx
					&& (instrument[currinstr].line[instry].cmd == '+' || instrument[currinstr].line[instry].cmd == '=')) {
						instrument[currinstr].line[instry].param = x;
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

void drawgui() {
	char buf[32];
	int songcols[] = {0, 1, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19, 21, 22};
	int trackcols[] = {0, 4, 5, 7, 8, 9, 11, 12, 13};
	int instrcols[] = {0, 2, 3};

	//erase();
	if (mode & MODE_EDIT)
	{
		mvaddstr(0, COLUMNS-22, "lock]  ");
	}
	else
	{
		mvaddstr(0, COLUMNS-22, "unlock]");
	}
	if (playsong||playtrack)
	{
		mvaddstr(1, COLUMNS-22, "stop play]");
	}
	else
	{
		mvaddstr(1, COLUMNS-22, "play]     ");
	}

	snprintf(buf, sizeof(buf), "%s                    ", filename);
	mvaddstr(2, 14, buf);
	if (mode & MODE_EDIT)
	{
		snprintf(buf, sizeof(buf), "^S)ongspeed: %d  ", songspeed);
		mvaddstr(3, 1, buf);
		snprintf(buf, sizeof(buf), "^T)racklength: %d  ", tracklen);
		mvaddstr(3, 30, buf);
		mvaddstr(3, COLUMNS - 23, "^W)rite");
	}
	else
	{
		snprintf(buf, sizeof(buf), "                    ");
		mvaddstr(3, 1, buf);
		mvaddstr(3, 30, buf);
		mvaddstr(3, COLUMNS - 23, "       ");
	}
	drawsonged(2, 6, LINES - 12);

	snprintf(buf, sizeof(buf), "%02x", currtrack);
	mvaddstr(5, 37, buf);
	drawtracked(31, 6, LINES - 8);

	snprintf(buf, sizeof(buf), "%02x", currinstr);
	mvaddstr(5, 58, buf);
	drawinstred(51, 6, LINES - 12);
	
	snprintf(buf, sizeof(buf), "%02d", octave);
	mvaddstr(5, COLUMNS - 7, buf);

	if (cmd[0])
	{
		snprintf(buf, sizeof(buf), "new %s? %s                      ", cmd, cmdinput);
	}
	else
	{
		if (alert[0])
			snprintf(buf, sizeof(buf), "%s                                ", alert);	
		else
		{
			memset(buf, 32, sizeof(buf)-1);
			buf[31] = 0;
		}
	}
	mvaddstr(LINES-1, 1, buf);

	switch(currtab) {
		case 0:
			move(6 + songy - songoffs, 0 + 6 + songcols[songx]);
			break;
		case 1:
			move(6 + tracky - trackoffs, 29 + 6 + trackcols[trackx]);
			break;
		case 2:
			move(6 + instry - instroffs, 49 + 6 + instrcols[instrx]);
			break;
	}

	refresh();
}

void redrawgui() {
	erase();
	mvaddstr(0, 0, " music chip tracker");

	mvaddstr(0, COLUMNS-29, "enter [");
	mvaddstr(1, COLUMNS-29, "space [");
	
	mvaddstr(2, 1, "^F)ilename:");
	
	mvaddstr(3, COLUMNS - 30, "^O)pen");
	
	mvaddstr(5, 1, "Song L1:\x1f\x1e R1:\x1f\x1e L2:\x1f\x1e R2:\x1f\x1e");

	mvaddstr(5, 31, "Track    {}");
	mvaddstr(5, 51, "Instr.    []");
	mvaddstr(5, COLUMNS - 15, "Octave:    <>");
	drawgui();
}

void game_frame() {
	playroutine();
	drawgui();
	handleinput();
}

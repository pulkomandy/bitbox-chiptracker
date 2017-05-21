#include "stuff.h"
#include "lib/chiptune/chiptune.h"
#include "lib/chiptune/player.h"
#include "lib/chiptune/player_internals.h"

#include <string.h>

//static u16 callbackwait;

volatile u8 test;
volatile u8 testwait;

u8 songwait;
u8 trackpos;
u16 songpos;

int tracklen = 32;
u8 songspeed = 4;

u8 playsong;
u8 playtrack;

struct instrument instrument[NINST];
//struct track track[NTRACK];
struct trackline tracking[NTRACKLINE];
struct songline song[256];

struct channel channel[8];

void silence() {
	for(u8 i = 0; i < 8; i++) {
		osc[i].volume = 0;
		channel[i].volumed = 0;
	}
	playsong = 0;
	playtrack = 0;
}

void readsong(int pos, uint8_t ch, u8 *dest) {
	dest[0] = song[pos].track[ch];
	dest[1] = song[pos].transp[ch];
}

void readtrack(uint8_t num, uint8_t pos, struct trackline *tl) {
	tl->note = track(num, pos)->note;
	tl->instr = track(num, pos)->instr;
	tl->cmd[0] = track(num, pos)->cmd[0];
	tl->cmd[1] = track(num, pos)->cmd[1];
	tl->param[0] = track(num, pos)->param[0];
	tl->param[1] = track(num, pos)->param[1];
}

void readinstr(uint8_t num, uint8_t pos, u8 *il) {
	if(pos >= instrument[num].length) {
		il[0] = 0;
		il[1] = 0;
		il[2] = 0;
		il[3] = 0;
	} else {
		il[0] = instrument[num].line[pos].cmd;
		il[1] = instrument[num].line[pos].param;

		il[2] = instrument[num].line[pos].cmd2;
		il[3] = instrument[num].line[pos].param2;
	}
}


void myruncmd(u8 ch, u8 cmd, u8 param, u8 context) {
	if (context == 2)
		ch += 4;

	switch(cmd) {
		// Commands affecting channel
		case 0:
			channel[ch].inum = 0;
			break;
		case 'f':
			channel[ch].volumed = param;
			break;
		case 'i':
			channel[ch].inertia = param << 1;
			break;
		case 'j':
			channel[ch].iptr = param;
			break;
		case 'l':
			channel[ch].bendd = param;
			break;
		case 'm':
			channel[ch].dutyd = param << 6;
			break;
		case 't':
			if (!context)
				channel[ch].iwait = param;
			else 
				songspeed = param;
			break;
		case '+':
			channel[ch].inote = param + channel[ch].tnote - 12 * 4;
			break;
		case '=':
			if (!context)
				channel[ch].inote = param;
			else
				channel[ch].lastinstr = param;
			break;
		case '~':
			if(channel[ch].vdepth != (param >> 4)) {
				channel[ch].vpos = 0;
			}
			channel[ch].vdepth = param >> 4;
			channel[ch].vrate = param & 15;
		break;

		// Commands affecting oscillator
		case 'b':
			osc[ch].bitcrush = param & 0x7;
			break;
		case 'd':
			osc[ch].duty = param << 8;
			break;
		case 'v':
			osc[ch].volume = param;
			break;
		case 'w':
			osc[ch].waveform = param;
			break;
	}
}

void iedplonk(int note, int instr) {
	channel[0].tnote = note;
	channel[0].inum = instr;
	channel[0].iptr = 0;
	channel[0].iwait = 0;
	channel[0].bend = 0;
	channel[0].bendd = 0;
	channel[0].volumed = 0;
	channel[0].dutyd = 0;
	channel[0].vdepth = 0;

	channel[4].tnote = note;
	channel[4].inum = instr;
	channel[4].iptr = 0;
	channel[4].iwait = 0;
	channel[4].bend = 0;
	channel[4].bendd = 0;
	channel[4].volumed = 0;
	channel[4].dutyd = 0;
	channel[4].vdepth = 0;
}

void startplaytrack(int t) {
	channel[0].tnum = t;
	channel[1].tnum = t;
	channel[2].tnum = 0;
	channel[3].tnum = 0;
	channel[4].tnum = t;
	channel[5].tnum = t;
	channel[6].tnum = 0;
	channel[7].tnum = 0;
	trackpos = 0;
	songwait = 0;
	playtrack = 1;
	playsong = 0;
}

void startplaysong(int p) {
	songpos = p;
	trackpos = 0;
	songwait = 0;
	playtrack = 0;
	playsong = 1;
}

void playroutine() {			// called at 50 Hz
	u8 ch;
	if(playtrack || playsong) {
		if(songwait) {
			songwait--;
		} else {
			songwait = songspeed;

			if(!trackpos) {
				if(playsong) {
					if(songpos >= songlen) {
						playsong = 0;
					} else {
						for(ch = 0; ch < 4; ch++) {
							u8 tmp[2];

							readsong(songpos, ch, tmp);
							channel[ch].tnum = tmp[0];
							channel[ch].transp = tmp[1];

							channel[ch+4].tnum = tmp[0];
							channel[ch+4].transp = tmp[1];
						}
						songpos++;
					}
				}
			}

			if(playtrack || playsong) {
				for(ch = 0; ch < 4; ch++) {
					if(channel[ch].tnum) {
						struct trackline tl;
						u8 instr = 0;
						readtrack(channel[ch].tnum, trackpos, &tl);
						if(tl.note) {
							channel[ch].tnote = tl.note + channel[ch].transp;
							channel[ch+4].tnote = tl.note + channel[ch+4].transp;
							instr = channel[ch].lastinstr;
						}
						if(tl.instr) {
							instr = tl.instr;
						}
						if(instr) {
							channel[ch].lastinstr = instr;
							channel[ch].inum = instr;
							channel[ch].iptr = 0;
							channel[ch].iwait = 0;
							channel[ch].bend = 0;
							channel[ch].bendd = 0;
							channel[ch].volumed = 0;
							channel[ch].dutyd = 0;
							channel[ch].vdepth = 0;

							channel[ch+4].lastinstr = instr;
							channel[ch+4].inum = instr;
							channel[ch+4].iptr = 0;
							channel[ch+4].iwait = 0;
							channel[ch+4].bend = 0;
							channel[ch+4].bendd = 0;
							channel[ch+4].volumed = 0;
							channel[ch+4].dutyd = 0;
							channel[ch+4].vdepth = 0;
						}
						if(tl.cmd[0])
							myruncmd(ch, tl.cmd[0], tl.param[0], 1);
						if(tl.cmd[1])
							myruncmd(ch, tl.cmd[1], tl.param[1], 2);
					}
				}

				trackpos++;
				if (trackpos == tracklen)
					trackpos = 0;
			}
		}
	}

	for(ch = 0; ch < 8; ch++) {
		s16 vol;
		u16 duty;
		u16 slur;

		while(channel[ch].inum && !channel[ch].iwait) {
			u8 il[4];

			readinstr(channel[ch].inum, channel[ch].iptr, il);
			channel[ch].iptr++;

			//myruncmd(ch, packcmd(il[0]), il[1], 0);
			myruncmd(ch, ch < 4 ? il[0] : il[2], ch < 4 ? il[1] : il[3], 0);
		}
		if(channel[ch].iwait) channel[ch].iwait--;

		if(channel[ch].inertia) {
			s16 diff;

			slur = channel[ch].slur;
			diff = chip_freqtable[channel[ch].inote] - slur;
			//diff >>= channel[ch].inertia;
			if(diff > 0) {
				if(diff > channel[ch].inertia) diff = channel[ch].inertia;
			} else if(diff < 0) {
				if(diff < -channel[ch].inertia) diff = -channel[ch].inertia;
			}
			slur += diff;
			channel[ch].slur = slur;
		} else {
			slur = chip_freqtable[channel[ch].inote];
		}
		osc[ch].freq =
			slur +
			channel[ch].bend +
			((channel[ch].vdepth * chip_sinetable[channel[ch].vpos & 63]) >> 2);
		channel[ch].bend += channel[ch].bendd;
		vol = osc[ch].volume + channel[ch].volumed;
		if(vol < 0) vol = 0;
		if(vol > 255) vol = 255;
		osc[ch].volume = vol;

		duty = osc[ch].duty + channel[ch].dutyd;
		if(duty > 0xe000) duty = 0x2000;
		if(duty < 0x2000) duty = 0xe000;
		osc[ch].duty = duty;

		channel[ch].vpos += channel[ch].vrate;
	}
}

void initchip() {
	songwait = 0;
	trackpos = 0;
	playsong = 0;
	playtrack = 0;

	osc[0].volume = 0;
	channel[0].inum = 0;
	osc[1].volume = 0;
	channel[1].inum = 0;
	osc[2].volume = 0;
	channel[2].inum = 0;
	osc[3].volume = 0;
	channel[3].inum = 0;
	channel[4].inum = 0;
	channel[5].inum = 0;
	channel[6].inum = 0;
	channel[7].inum = 0;

	// Initialize the second set of oscillators to an "idle" value for backwards compatibility.
	osc[4].volume = 0;
	osc[5].volume = 0;
	osc[6].volume = 0;
	osc[7].volume = 0;

	osc[4].freq = 0;
	osc[4].phase = 0;
	osc[5].freq = 0;
	osc[5].phase = 0;
	osc[6].freq = 0;
	osc[6].phase = 0;
	osc[7].freq = 0;
	osc[7].phase = 0;
}


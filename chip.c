#include "stuff.h"
#include <chiptune_engine.h>
#include <chiptune_player.h>

//static u16 callbackwait;

u8 playtrack;

void silence() {
	u8 i;

	for(i = 0; i < 4; i++) {
		osc[i].volume = 0;
	}
	playsong = 0;
	playtrack = 0;
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
}

void startplaytrack(int t) {
	channel[0].tnum = t;
	channel[1].tnum = 0;
	channel[2].tnum = 0;
	channel[3].tnum = 0;
	trackpos = 0;
	trackwait = 0;
	playtrack = 1;
	playsong = 0;
}

void startplaysong(int p) {
	songpos = p;
	trackpos = 0;
	trackwait = 0;
	playtrack = 0;
	playsong = 1;
}

void playroutine() {			// called at 50 Hz
	u8 ch;

	if(playtrack || playsong) {
		if(trackwait) {
			trackwait--;
		} else {
			trackwait = 4;

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
						}
						if(tl.cmd[0])
							runcmd(ch, packcmd(tl.cmd[0]), tl.param[0]);
						/*if(tl.cmd[1])
							runcmd(ch, packcmd(tl.cmd[1]), tl.param[1]);*/
					}
				}

				trackpos++;
				trackpos &= 31;
			}
		}
	}

	for(ch = 0; ch < 4; ch++) {
		s16 vol;
		u16 duty;
		u16 slur;

		while(channel[ch].inum && !channel[ch].iwait) {
			u8 il[2];

			readinstr(channel[ch].inum, channel[ch].iptr, il);
			channel[ch].iptr++;

			runcmd(ch, packcmd(il[0]), il[1]);
		}
		if(channel[ch].iwait) channel[ch].iwait--;

		if(channel[ch].inertia) {
			s16 diff;

			slur = channel[ch].slur;
			diff = freqtable[channel[ch].inote] - slur;
			//diff >>= channel[ch].inertia;
			if(diff > 0) {
				if(diff > channel[ch].inertia) diff = channel[ch].inertia;
			} else if(diff < 0) {
				if(diff < -channel[ch].inertia) diff = -channel[ch].inertia;
			}
			slur += diff;
			channel[ch].slur = slur;
		} else {
			slur = freqtable[channel[ch].inote];
		}
		osc[ch].freq =
			slur +
			channel[ch].bend +
			((channel[ch].vdepth * sinetable[channel[ch].vpos & 63]) >> 2);
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
	trackwait = 0;
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
}


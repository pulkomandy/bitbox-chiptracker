#include "fatfs/ff.h"
#include <stdarg.h> // va_list
#include <stdio.h>
#include <string.h> // memset
#include "stuff.h"
#include <bitbox.h>


char filename[13];
u16 numtracks;

static FATFS fat_fs;
static FIL fat_file;
static FRESULT fat_result;

void initio()
{
	fat_result = f_mount(&fat_fs, "", 1);
}

void savefile(char *fname) {
	fat_result = f_open(&fat_file, fname, FA_WRITE | FA_OPEN_ALWAYS);
	if (fat_result != FR_OK) {
		switch (fat_result) {
		case (FR_DISK_ERR):
			setalert("low level error!"); break;
		case (FR_INT_ERR):
			setalert("assertion error"); break;
		case (FR_NOT_READY):
			setalert("physical drive not ready"); break;
		case (FR_NO_FILE):
			setalert("could not find file"); break;
		case (FR_NO_PATH):
			setalert("could not find path"); break;
		case (FR_INVALID_NAME):
			setalert("invalid name format"); break;
		case (FR_DENIED):
			setalert("denied access (full?)"); break;
		case (FR_EXIST):
			setalert("prohibited access"); break;
		case (FR_INVALID_OBJECT):
			setalert("invalid file/dir obj"); break;
		case (FR_WRITE_PROTECTED):
			setalert("drive is write protected"); break;
		case (FR_INVALID_DRIVE):
			setalert("drive number is invalid"); break;
		case (FR_NOT_ENABLED):
			setalert("no work area in volume"); break;
		case (FR_NO_FILESYSTEM):
			setalert("not valid FAT volume"); break;
		case (FR_MKFS_ABORTED):
			setalert("f_mkfs abort?"); break;
		case (FR_TIMEOUT):
			setalert("timeout"); break;
		case (FR_LOCKED):
			setalert("locked due to sharing"); break;
		case (FR_NOT_ENOUGH_CORE):
			setalert("LFN buffer no-allocate"); break;
		case (FR_TOO_MANY_OPEN_FILES):
			setalert("too many open files"); break;
		case (FR_INVALID_PARAMETER):	
			setalert("invalid parameter"); break;
		default:
			setalert("weird, some other error!");
		}
		return;
	}

	int i, j;
	f_printf(&fat_file, "musicchip tune\nversion 1\n\n");
	if(tracklen != 32)
	{
		 f_printf(&fat_file, "tracklength %02x\n\n", tracklen);
	}
	f_printf(&fat_file, "speed %02x\n\n", songspeed);
	for(i = 0; i < songlen; i++) {
		f_printf(&fat_file, "songline %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			i,
			song[i].track[0],
			song[i].transp[0],
			song[i].track[1],
			song[i].transp[1],
			song[i].track[2],
			song[i].transp[2],
			song[i].track[3],
			song[i].transp[3]);
	}
	f_printf(&fat_file, "\n");
	struct trackline *tl = track(1,0);
	for(i = 1; i <= numtracks; i++) {
		for(j = 0; j < tracklen; j++) {
			if(tl->note || tl->instr || tl->cmd[0] || tl->cmd[1]) {
				f_printf(&fat_file, "trackline %02x %02x %02x %02x %02x %02x %02x %02x\n",
					i,
					j,
					tl->note,
					tl->instr,
					tl->cmd[0],
					tl->param[0],
					tl->cmd[1],
					tl->param[1]);
			}
			tl++;
		}
	}
	f_printf(&fat_file, "\n");
	for(i = 1; i < NINST; i++) {
		if(instrument[i].length > 1) {
			for(j = 0; j < instrument[i].length; j++) {
				f_printf(&fat_file, "instrumentline %02x %02x %02x %02x\n",
					i,
					j,
					instrument[i].line[j].cmd,
					instrument[i].line[j].param);
			}
		}
	}

	f_close(&fat_file);
	setalert("file saved!");
}

bool loadfile(char *fname) {
	clear_song();
	fat_result = f_open(&fat_file, fname, FA_READ);
	strcpy(filename, fname);
	if(fat_result == FR_OK) {
		setalert("Loading...");
	} else {
		switch (fat_result) {
		case (FR_DISK_ERR):
			setalert("low level error!"); break;
		case (FR_INT_ERR):
			setalert("assertion error"); break;
		case (FR_NOT_READY):
			setalert("physical drive not ready"); break;
		case (FR_NO_FILE):
			setalert("could not find file"); break;
		case (FR_NO_PATH):
			setalert("could not find path"); break;
		case (FR_INVALID_NAME):
			setalert("invalid name format"); break;
		case (FR_DENIED):
			setalert("denied access (full?)"); break;
		case (FR_EXIST):
			setalert("prohibited access"); break;
		case (FR_INVALID_OBJECT):
			setalert("invalid file/dir obj"); break;
		case (FR_WRITE_PROTECTED):
			setalert("drive is write protected"); break;
		case (FR_INVALID_DRIVE):
			setalert("drive number is invalid"); break;
		case (FR_NOT_ENABLED):
			setalert("no work area in volume"); break;
		case (FR_NO_FILESYSTEM):
			setalert("not valid FAT volume"); break;
		case (FR_MKFS_ABORTED):
			setalert("f_mkfs abort?"); break;
		case (FR_TIMEOUT):
			setalert("timeout"); break;
		case (FR_LOCKED):
			setalert("locked due to sharing"); break;
		case (FR_NOT_ENOUGH_CORE):
			setalert("LFN buffer no-allocate"); break;
		case (FR_TOO_MANY_OPEN_FILES):
			setalert("too many open files"); break;
		case (FR_INVALID_PARAMETER):	
			setalert("invalid parameter"); break;
		default:
			setalert("weird, some other error!");
		}
		return false;
	}

	char buf[48];
	int cmd[3];
	int i1, i2, trk[4], transp[4], param[3], note, instr;
	int i;

	// Restore default values for old songs that don't have one
	tracklen = 32;
	songspeed = 4;

	while (f_gets(buf, sizeof(buf), &fat_file)) {
		if(9 == rsscanf(buf, "songline %x %x %x %x %x %x %x %x %x",
			&i1,
			&trk[0],
			&transp[0],
			&trk[1],
			&transp[1],
			&trk[2],
			&transp[2],
			&trk[3],
			&transp[3])) {

			for(i = 0; i < 4; i++) {
				song[i1].track[i] = trk[i];
				song[i1].transp[i] = transp[i];
			}
			if(songlen <= i1) songlen = i1 + 1;
		} else if(8 == rsscanf(buf, "trackline %x %x %x %x %x %x %x %x",
			&i1,
			&i2,
			&note,
			&instr,
			&cmd[0],
			&param[0],
			&cmd[1],
			&param[1])) {

			if (i1 > numtracks)
				numtracks = i1;
			track(i1, i2)->note = note;
			track(i1, i2)->instr = instr;
			for(i = 0; i < 2; i++) {
				track(i1, i2)->cmd[i] = cmd[i];
				track(i1, i2)->param[i] = param[i];
			}
		} else if(4 == rsscanf(buf, "instrumentline %x %x %x %x",
			&i1,
			&i2,
			&cmd[0],
			&param[0])) {

			instrument[i1].line[i2].cmd = cmd[0];
			instrument[i1].line[i2].param = param[0];
			if(instrument[i1].length <= i2) instrument[i1].length = i2 + 1;
		} else if(1 == rsscanf(buf, "tracklength %x", &i)) {
			tracklen = i;
		} else if(1 == rsscanf(buf, "speed %x", &i)) {
			songspeed = i;
		}
	}

	f_close(&fat_file);
	return true;
}

void clear_song()
{
	for(int i = 1; i < NINST; i++) {
		instrument[i].length = 1;
		instrument[i].line[0].cmd = '0';
		instrument[i].line[0].param = 0;
	}
	memset(tracking, 0, sizeof(tracking));
	memset(song, 0, sizeof(song));
	songlen = 1;
	tracklen = 32; // default
	numtracks = 0;
}

int realign_tracks(int new_tracklen)
{
	if (new_tracklen == tracklen)
		return 0;
	// return 0 if everything went fine,
	// return 1 if we lost a few tracks since we have a huge new tracklen
	int returnvalue = 0;
	if (new_tracklen > tracklen)
	{
		for (int fix_index=numtracks; fix_index>1; fix_index--)
		{
			if (fix_index*new_tracklen >= NTRACKLINE)
			{
				returnvalue = 1;
				numtracks--;
			}
			else
			{
				struct trackline *src = &tracking[(fix_index-1)*tracklen];
				struct trackline *dest = &tracking[(fix_index-1)*new_tracklen];
				// copy the original track into the new spot
				memcpy(dest, src, tracklen*sizeof(struct trackline));
				dest += tracklen;
				// zero out the rest of the track
				memset(dest, 0, (new_tracklen-tracklen)*sizeof(struct trackline));
			}
		}
		// zero out the rest of the track 1
		memset(&tracking[tracklen], 0, (new_tracklen-tracklen)*sizeof(struct trackline));
	}
	else
	{
		for (int fix_index=2; fix_index<=numtracks; fix_index++)
		{
			struct trackline *src = &tracking[(fix_index-1)*tracklen];
			struct trackline *dest = &tracking[(fix_index-1)*new_tracklen];
			// copy the original track into the new spot
			memcpy(dest, src, new_tracklen*sizeof(struct trackline));
		}

	}
	tracklen = new_tracklen;
	return returnvalue; // if everything is alright
}


// Reduced version of scanf (%d, %x, %c, %n are supported)
// thanks to https://groups.google.com/d/msg/comp.arch.fpga/ImYAZ6X_Wm4/WE-gYX-mNSgJ
// %d dec integer (E.g.: 12)
// %x hex integer (E.g.: 0xa0)
// %b bin integer (E.g.: b1010100010)
// %n hex, de or bin integer (e.g: 12, 0xa0, b1010100010)
// %c any character
//
int rsscanf(const char* str, const char* format, ...)
{
	va_list ap;
	int value, tmp;
	int count;
	int pos;
	char neg, fmt_code;
	//const char* pf;

	va_start(ap, format);

	for (/*pf = format, */count = 0; *format != 0 && *str != 0; format++, str++)
	{
		while (*format == ' ' && *format != 0)
			format++;
		if (*format == 0)
			break;

		while (*str == ' ' && *str != 0)
			str++;
		if (*str == 0)
			break;

		if (*format == '%')
		{
			format++;
			if (*format == 'n')
			{
				if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
				{
					fmt_code = 'x';
					str += 2;
				}
				else
				if (str[0] == 'b')
				{
					fmt_code = 'b';
					str++;
				}
				else
					fmt_code = 'd';
			}
			else
				fmt_code = *format;

			switch (fmt_code)
			{
			case 'x':
			case 'X':
				for (value = 0, pos = 0; *str != 0; str++, pos++)
				{
					if ('0' <= *str && *str <= '9')
						tmp = *str - '0';
					else
					if ('a' <= *str && *str <= 'f')
						tmp = *str - 'a' + 10;
					else
					if ('A' <= *str && *str <= 'F')
						tmp = *str - 'A' + 10;
					else
						break;

					value *= 16;
					value += tmp;
				}
				if (pos == 0)
						return count;
				*(va_arg(ap, int*)) = value;
				count++;
				break;

			case 'b':
				for (value = 0, pos = 0; *str != 0; str++, pos++)
				{
					if (*str != '0' && *str != '1')
						break;
					value *= 2;
					value += *str - '0';
				}
				if (pos == 0)
					return count;
				*(va_arg(ap, int*)) = value;
				count++;
				break;

			case 'd':
				if (*str == '-')
				{
					neg = 1;
					str++;
				}
				else
					neg = 0;
				for (value = 0, pos = 0; *str != 0; str++, pos++)
				{
					if ('0' <= *str && *str <= '9')
						value = value*10 + (int)(*str - '0');
					else
						break;
				}
				if (pos == 0)
					return count;
				*(va_arg(ap, int*)) = neg ? -value : value;
				count++;
				break;

			case 'c':
				*(va_arg(ap, char*)) = *str;
				count++;
				break;

			default:
				return count;
			}
		}
		else
		{
			if (*format != *str)
				break;
		}
	}

	va_end(ap);

	return count;
}

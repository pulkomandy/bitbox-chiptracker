USE_SDCARD = 1      # allow use of SD card for io

NAME = chiptracker
GAME_C_FILES = main.c chip.c gui.c io.c extrafat.c \
	lib/chiptune/chiptune.c  lib/chiptune/player.c \
	lib/textmode/textmode.c \
	lib/events/events.c \


DEFINES += MAX_CHANNELS=4 FONT_W=8 FONT_H=16

BITBOX ?= ../sdk

include $(BITBOX)/kernel/bitbox.mk

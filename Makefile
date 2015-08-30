USE_SDCARD = 1      # allow use of SD card for io

NAME = chiptracker
GAME_C_FILES = main.c chip.c gui.c io.c extrafat.c
VGA_SIMPLE_MODE = 10 # textmode with colors
USE_CHIPTUNE = 1

BITBOX ?= ../bitbox

include $(BITBOX)/lib/bitbox.mk

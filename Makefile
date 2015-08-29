NAME = chiptracker
GAME_C_FILES = main.c chip.c gui.c io.c
VGA_SIMPLE_MODE = 10 # textmode with colors
USE_CHIPTUNE = 1

BITBOX ?= ../bitbox

include $(BITBOX)/lib/bitbox.mk

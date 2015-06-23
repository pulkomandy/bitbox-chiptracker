NAME = chiptracker
GAME_C_FILES = main.c chip.c gui.c
VGA_SIMPLE_MODE = 10 # textmode with colors

BITBOX ?= ../bitbox

include $(BITBOX)/lib/bitbox.mk

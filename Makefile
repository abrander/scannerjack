all: scannerjack

scannerjack: main.c uniden.c uniden.h
	 gcc -g -o scannerjack main.c uniden.c `pkg-config --cflags --libs gtk+-3.0`

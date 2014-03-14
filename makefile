# Makefile for Sound Blaster (using BSD-386 device driver)
#
# $Author: steve $
# $Id: Makefile,v 1.2 1992/06/24 06:29:59 steve Exp steve $
# $Revision: 1.2 $

CC = gcc
INSTALL_DIR = $(HOME)/bin
O = .obj
CFLAGS = 
MAIN_OPTS = -O2   #Try -O2 if you have gcc 2.x
COPTS = -c -O2 -Wall
MACHINE = soundblaster
OBJECTS = main$O $(MACHINE)_audio$O read$O commands$O \
          audio$O automaton$O player$O easyfont$O

ICONS = tracker.ico pause.ico record.ico play.ico stop.ico rewind.ico fastforward.ico

all: tracker.exe lcd.fon

tracker.exe:  ${OBJECTS} tracker.res tracker.def
	link386 ${OBJECTS}, tracker.exe,,,tracker.def
	rc tracker.res tracker.exe

lcd.fon: lcd$O lcd.def lcd.res
	link386 lcd$O, lcd.fon,,,lcd.def
	rc lcd.res lcd.fon

lcd$O: lcd.c
	$(CC) -c lcd.c

lcd.res: lcd.rc lcd.fnt
	rc -r lcd.rc

main$O: main.c defs.h os2defs.h tracker.h
	$(CC) ${COPTS} main.c

$(MACHINE)_audio$O: $(MACHINE)_audio.c
	$(CC) ${COPTS} $(MACHINE)_audio.c

audio$O: audio.c
	$(CC) ${COPTS} audio.c

automaton$O: automaton.c defs.h
	$(CC) ${COPTS} automaton.c

player$O: player.c defs.h os2defs.h
	$(CC) ${COPTS} player.c

read$O: read.c  defs.h
	$(CC) ${COPTS} read.c

commands$O: commands.c defs.h
	$(CC) ${COPTS} commands.c

easyfont$O: easyfont.c easyfont.h
	$(CC) ${COPTS} easyfont.c

tracker.res: tracker.rc tracker.h lcd.fnt $(ICONS)
	rc -r tracker.rc

#machine.h: $(MACHINE).h
#	cp $(MACHINE).h machine.h

clean:
	del *.obj *.exe *.s *.map *.lst



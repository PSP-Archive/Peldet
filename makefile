TARGET = peldet
APPOBJS= main.o libs/nlh.o libs/loadutil.o libs/stubs.o libs/p_sprint.o support.o pic.o render.o protocol/telnet.o protocol/irc.o

PSP_EBOOT_PIC1 = PIC1.PNG

OBJS = $(APPOBJS)

INCDIR = 
#CFLAGS = -O2 -G0 -Wall
CFLAGS = -O2 -G0 -g -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -c

LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspwlan -lpsppower
#USE_PSPSDK_LIBC = 1

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = peldet 0.8

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

install:
	mount /mnt/psp && cp peldet peldet% /mnt/psp/psp/game -r && umount /mnt/psp

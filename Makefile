ifndef FD32
FD32 = ../../fd32
endif

include $(FD32)/mk/djgpp.mk

.PHONY: all clean

all: command.exe

clean:
	$(RM) command.exe

command.exe: command.c
	$(CC) -O3 -Xlinker -Ttext -Xlinker 0x300000 command.c -s -o command.exe

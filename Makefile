# Project: FreeDOS-32 command
# Makefile for DJGPP and Mingw32

CC = i586-pc-msdosdjgpp-gcc
C_OPT = -Wall -O2 -finline-functions
LINK_OPT =
OBJS = command.o cmdbuf.o

.PHONY: all clean

all: command.exe

clean:
	$(RM) command.exe
	$(RM) *.o

command.exe: $(OBJS)
	$(CC) $(LINK_OPT) $(OBJS) -o command.exe

# Common rules
%.o : %.c
	$(REDIR) $(CC) $(C_OPT) $(C_OUTPUT) -c $<

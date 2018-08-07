# Project: FreeDOS-32 command
# Makefile for DJGPP and Mingw32

CC = i586-pc-msdosdjgpp-gcc
C_OPT = -Wall -O2 -finline-functions
LINK_OPT =
OBJS = command.o cmdbuf.o
CMD = comcom32.exe

.PHONY: all clean

all: $(CMD)

clean:
	$(RM) $(CMD)
	$(RM) *.o

$(CMD): $(OBJS)
	$(CC) $(LINK_OPT) $(OBJS) -o $(CMD)

# Common rules
%.o : %.c
	$(REDIR) $(CC) $(C_OPT) $(C_OUTPUT) -c $<

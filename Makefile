ifndef FD32
FD32 = ../../fd32
endif

include $(FD32)/mk/djgpp.mk

C_OPT = -Wall -O3 -finline-functions
LINK_OPT = -Ttext 0x300000
OBJS = command.o cmdbuf.o

.PHONY: all clean

all: command.exe

clean:
	$(RM) command.exe
	$(RM) *.o

command.exe: $(OBJS)
	$(CC) $(LINK_OPT) $(OBJS) -s -o command.exe

# Common rules
%.o : %.c
	$(REDIR) $(CC) $(C_OPT) $(C_OUTPUT) -c $<

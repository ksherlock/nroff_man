
CFLAGS = -Wall -g -std=c89
CPPFLAGS = -D_DEFAULT_SOURCE
LDLIBS = -ltermcap
OBJS = main.o read1.o read2.o read3.o chars.o

ifeq ($(MSYSTEM),MSYS)
	LDLIBS = -lncurses
endif

nroff_man: $(OBJS)
	$(CC) $^ $(LDLIBS) -o $@

read3.o : read3.c man.h
read2.o : read1.c man.h
read1.o : read1.c man.h escape.h
chars.o : chars.c chars.h

.PHONY: clean

clean:
	$(RM) $(OBJS)

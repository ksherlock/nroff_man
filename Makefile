
CFLAGS = -Wall  -Wno-pointer-sign -g -std=c99  -pedantic
CPPFLAGS = -D_DEFAULT_SOURCE
LDLIBS = -ltermcap
OBJS = main.o read1.o read2.o read3.o chars.o render.o tbl.o

ifeq ($(MSYSTEM),MSYS)
	LDLIBS = -lncurses
endif

nroff_man: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

main.c : man.h xtokens.h
read3.o : read3.c man.h xtokens.h
read2.o : read1.c man.h xtokens.h
read1.o : read1.c man.h escape.h xtokens.h
render.o : render.c man.h xtokens.h
chars.o : chars.c man.chars.h man.strings.h
tbl.o: tbl.c man.h xtokens.h

.PHONY: clean

clean:
	$(RM) $(OBJS)

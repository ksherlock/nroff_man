
CFLAGS = -Wall -g -std=c89
CPPFLAGS = -D_BSD_SOURCE
LDLIBS = -ltermcap
OBJS = main.o read1.o read2.o read3.o

nroff_man: $(OBJS)
	$(CC) $^ $(LDLIBS) -o $@

.PHONY: clean

clean:
	$(RM) $(OBJS)

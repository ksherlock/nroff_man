
CFLAGS = -Wall -g -std=c89
LDLIBS = -ltermcap

nroff_man:  main.o read1.o read2.o read3.o
	$(CC) $^ $(LDLIBS) -o $@
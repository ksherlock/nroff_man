

# Program name
PROG	= nroff.man
SRCS	= main.c read1.c read2.c read3.c chars.c
MAIN	= main
MAINSRC	= main.c

# Add -D__STACK_CHECK__	to print, on exit, the amount of stack used
# DEFINES	+= -D__STACK_CHECK__
CFLAGS += -w-1

OPTIMIZE = 78
BINDIR	= /usr/bin
LDLIBS	= -l/usr/lib/libtermcap.204

.INCLUDE:	/src/gno/prog.mk

               
#
# additional dependancies
#
main.o::	man.h xtokens.h
read1.o::	man.h xtokens.h
read2.o::	man.h xtokens.h
read3.o::	man.h xtokens.h
chars.o::	man.chars.h man.strings.h

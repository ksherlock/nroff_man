#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "man.h"

#ifdef __STACK_CHECK__
#include <gno/gno.h>
#endif

static void usage(int rv) {
	fputs(
		"nroff.man [-bih] [-W level] [-I os=name] [-t terminal] [-T output] [file ...]\n"
		" -t terminal   set terminal type\n"
		" -T output     set output type (termcap, ascii, plain)\n"
		" -h            help\n",

		stdout);


	exit(rv);
}


static int parseT(const char *arg) {
	/* */
	if (!arg || !*arg) return -1;
	if (arg[0] == 't' && !strcmp(arg, "termcap")) return FMT_TERMCAP;
	if (arg[0] == 'a' && !strcmp(arg, "ascii")) return FMT_ASCII;
	if (arg[0] == 'p' && !strcmp(arg, "plain")) return FMT_PLAIN;
	return -1;
}

#ifdef __STACK_CHECK__
static void
printStack (void) {
    fprintf(stderr, "stack usage: %d bytes\n", _endStackCheck());
}
#endif

int main(int argc, char **argv) {

	int i;
	int ch;

#ifdef __STACK_CHECK__
    _beginStackCheck();
    atexit(printStack);
#endif

	while (( ch = getopt(argc, argv, "m:hW:I:ibt:T:")) != -1) {
		switch(ch) {
			case '?': case ':':
			default: usage(1);
			case 'h': usage(0);
			case 'i': flags.i = 1; break;
			case 'b': flags.b = 1; break;
			case 'W':
				if (isdigit(*optarg))
					flags.W = *optarg - '0';
				else
					warnx("-W %s: Bad argument", optarg);
				break;
			case 'I':
				if (strncmp(optarg, "os=", 3) == 0)
					flags.os = optarg + 3;
				else
					warnx("-I %s: Bad argument", optarg);
				break;
			case 'm': /* silently accept -man */
				break;

			case 't':
				flags.t = optarg;
				break;
			case 'T':
				flags.T = parseT(optarg);
				if (flags.T < 0) usage(1);

				break;
		}
	}

	argc -= optind;
	argv += optind;


	if (flags.T == FMT_TERMCAP)
		tc_init();

	if (argc == 0) {
		man(stdin, "stdin");
	} else {
		for (i = 0; i < argc; ++i) {
			char *cp = argv[i];
			FILE *fp = fopen(cp, "r");
			if (!fp) err(1, "Unable to open %s", cp);
			man(fp, cp);
			fclose(fp);
		}
	}

	return 0;
}

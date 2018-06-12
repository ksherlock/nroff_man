#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "man.h"


static void usage(void) {
	fputs(
		"nroff.man [-bih] [-W level] [-I os=name] [file ...]\n",
		stdout);
}

int main(int argc, char **argv) {

	int i;
	int ch;

	while (( ch = getopt(argc, argv, "hW:I:ib")) != -1) {
		switch(ch) {
			case '?': case ':':
			default: usage(); exit(1);
			case 'h': usage(); exit(0);
			case 'i': flags.i = 1; break;
			case 'b': flags.b = 1; break;
			case 'W':
				if (isdigit(*optarg))
					flags.W = *optarg - '0';
				break;
			case 'I':
				if (strncmp(optarg, "os=", 3) == 0)
					flags.os = optarg + 3;
				break;
		}
	}

	argc -= optind;
	argv += optind;


	man_init();

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
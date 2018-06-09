#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <err.h>

#include "man.h"


const char *special_char(const char *cp) { return ""; }

static void usage(void) {

}

int main(int argc, char **argv) {

	int i;
	int ch;

	while (( ch = getopt(argc, argv, "hW:I:")) != -1) {
		switch(ch) {
			case '?':
			case ':':
			default:
				usage();
				exit(1);
			case 'h':
				usage();
				exit(0);
			case 'W':
				flags.W = 1;
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
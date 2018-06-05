#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>

#include "man.h"


const char *special_char(const char *cp) { return ""; }

static void usage(void) {

}

int main(int argc, char **argv) {

	int i;
	int ch;

	while (( ch = getopt(argc, argv, "h")) != -1) {
		switch(ch) {
			case '?':
			case ':':
			default:
				usage();
				exit(1);
			case 'h':
				usage();
				exit(0);
		}
	}

	argc -= optind;
	argv += optind;


	man_init();

	if (argc == 0) {
		man(stdin);
	} else {
		for (i = 0; i < argc; ++i) {
			char *cp = argv[i];
			FILE *fp = fopen(cp, "r");
			if (!fp) err(1, "Unable to open %s", cp);
			man(fp);
			fclose(fp);
		}
	}

	return 0;
}
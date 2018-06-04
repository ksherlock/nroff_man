#include <ctype.h>
#include <stdio.h>

#include "man.h"

static char buffer[4096];

static char *fonts(unsigned f1, unsigned f2) {
	unsigned i,j,k;
	unsigned char c;

	type = tkTEXT;
	for (i = 0, j = 0; i < argc; ++i) {
		const char *cp = argv[i];
		buffer[j++] = i & 0x01 ? f2 : f1;
		k = 0;
		while ((c = cp[k++])) buffer[j++] = c;
	}

	buffer[j++] = FONT_R;
	return buffer;
}

static char *font(unsigned f) {
	int i,j,k;
	buffer[0] = f;
	unsigned char c;

	type = tkTEXT;
	for(i = 0, j = 1; i <argc; ++i) {
		const char *cp = argv[i];
		if (i > 0) buffer[j++] = ' ';
		k = 0;
		while ((c = cp[k++])) buffer[j++] = c;
	}
	buffer[j++] = FONT_R;
	return buffer;
}

const char *read_text(void) {

	unsigned ff = 0;
	for(;;) {
		const char *cp = read_line();
		if (type == tkEOF) return NULL;

		switch(type) {
			case tkDT:
			case tkUR:
			case tkUE:
			case tkMT:
			case tkME:
			case tkAT:
			case tkUC:
				continue;

			/* next-line fonts */
			case tkB:
			case tkSB:
				ff = FONT_B;
				if (argc) return font(ff);
				continue;
			case tkR:
			case tkSM:
				ff = FONT_R;
				if (argc) return font(ff);
				continue;
			case tkI:
				ff = FONT_I;
				if (argc) return font(ff);
				continue;

			/* current-line fonts */
			case tkBI: return fonts(FONT_B, FONT_I);
			case tkBR: return fonts(FONT_B, FONT_R);
			case tkIB: return fonts(FONT_I, FONT_B);
			case tkIR: return fonts(FONT_I, FONT_R);
			case tkRI: return fonts(FONT_R, FONT_I);
			case tkRB: return fonts(FONT_R, FONT_B);

			case tkOP:
				if (argc == 0) continue;
				if (argc == 1)
					sprintf(buffer, "[%c%s%c]",
						FONT_B, argv[0], FONT_R
					);
				else
					sprintf(buffer, "[%c%s%c %c%s%c]",
						FONT_B, argv[0], FONT_R,
						FONT_I, argv[1], FONT_R);
				type = tkTEXT;
				return buffer;

			case tkTEXT:
				if (ff) {
					sprintf(buffer, "%c%s%c",
						ff, cp, FONT_R);
					return buffer;
				}
			default:
				return cp;
		}

	}



}
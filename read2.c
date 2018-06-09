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
	buffer[j++] = 0;
	return buffer;
}

static char *font(unsigned f) {
	int i,j,k;
	unsigned char c;

	buffer[0] = f;

	type = tkTEXT;
	for(i = 0, j = 1; i <argc; ++i) {
		const char *cp = argv[i];
		if (i > 0) buffer[j++] = ' ';
		k = 0;
		while ((c = cp[k++])) buffer[j++] = c;
	}
	buffer[j++] = FONT_R;
	buffer[j++] = 0;
	return buffer;
}

unsigned xstrcpy(char *dest, const char *src) {
	unsigned i = 0;
	while ((dest[i] = src[i])) ++i;
	return i;
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
			case tkSY:
				/* bold up argv0 but retain the type */
				if (argc) {
					unsigned i;
					buffer[0] = FONT_B;
					i = xstrcpy(buffer+1, argv[0]);
					/* pre-increment to adjust for buffer+1 */
					buffer[++i] = FONT_R;
					buffer[++i] = 0;
					argv[0] = buffer;
					return "";
				}

			/* current-line fonts */
			case tkBI: return fonts(FONT_B, FONT_I);
			case tkBR: return fonts(FONT_B, FONT_R);
			case tkIB: return fonts(FONT_I, FONT_B);
			case tkIR: return fonts(FONT_I, FONT_R);
			case tkRI: return fonts(FONT_R, FONT_I);
			case tkRB: return fonts(FONT_R, FONT_B);

			case tkOP: {
				unsigned i;
				if (argc == 0) continue;
					buffer[0] = '[';
					buffer[1] = FONT_B;
					i = 2 + xstrcpy(buffer+2, argv[0]);
					buffer[i++] = FONT_R;
					if (argc > 1) {
						buffer[i++] = NBSPACE;
						buffer[i++] = FONT_I;
						i += xstrcpy(buffer + i, argv[1]);
						buffer[i++] = FONT_R;
					}
					buffer[i++] = ']'; 
					buffer[i] = 0; 
				}
				type = tkTEXT;
				return buffer;

			case tkTEXT:
				if (ff) {
					unsigned i;
					buffer[0] = ff;
					i = xstrcpy(buffer+1, cp);
					/* pre-increment to adjust for buffer+1 */
					buffer[++i] = FONT_R;
					buffer[++i] = 0;
					ff = 0;
					return buffer;
				}
			default:
				return cp;
		}

	}



}
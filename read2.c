#include <ctype.h>
#include <stdio.h>
#include <err.h>

#include "man.h"

/*
 * how much buffer space do we need?
 * scenario 1:
 * .B
 * text...
 *
 * text is at MAN_BUFFER_SIZE, including null, so 2 extra bytes required
 * for font bytes.
 *
 * scenario 2:
 * .BR arg0 ... arg9
 *
 * if 1 arg, MAN_BUFFER_SIZE + 2 font bytes
 * if 9 args, MAN_BUFFER_SIZE - 9 null bytes + 18 font bytes + 1 null byte
 *
 * .B arg0 ... arg9
 * MAN_BUFFER_SIZE - 9 null bytes + 8 spaces + 1 null bytes + 2 font bytes.
 */
static unsigned char buffer[MAN_BUFFER_SIZE + 20];

static unsigned char *fonts(unsigned f1, unsigned f2) {
	unsigned i,j,k;
	unsigned char c;

	type = tkTEXT;
	for (i = 0, j = 0; i < argc; ++i) {
		const unsigned char *cp = argv[i];
		buffer[j++] = i & 0x01 ? f2 : f1;
		k = 0;
		while ((c = cp[k++])) buffer[j++] = c;
	}

	buffer[j++] = FONT_R;
	buffer[j++] = 0;
	return buffer;
}

static unsigned char *font(unsigned f) {
	int i,j,k;
	unsigned char c;

	buffer[0] = f;

	type = tkTEXT;
	for(i = 0, j = 1; i <argc; ++i) {
		const unsigned char *cp = argv[i];
		if (i > 0) buffer[j++] = ' ';
		k = 0;
		while ((c = cp[k++])) buffer[j++] = c;
	}
	buffer[j++] = FONT_R;
	buffer[j++] = 0;
	return buffer;
}

unsigned xstrcpy(unsigned char *dest, const unsigned char *src) {
	unsigned i = 0;
	while ((dest[i] = src[i])) ++i;
	return i;
}

const unsigned char *read_text(void) {

	unsigned ff = 0;
	for(;;) {
		const unsigned char *cp = read_line();
		if (type == tkEOF) return NULL;

		switch(type) {
			case tkDT:
				continue;
			case tkUR:
				if (flags.W >= 3) man_warnx(".UR is a non-standard GNU extension.");
				continue;
			case tkUE:
				if (flags.W >= 3) man_warnx(".UE is a non-standard GNU extension.");
				continue;
			case tkMT:
				if (flags.W >= 3) man_warnx(".MT is a non-standard GNU extension.");
				continue;
			case tkME:
				if (flags.W >= 3) man_warnx(".ME is a non-standard GNU extension.");
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
				if (flags.W >= 3) man_warnx(".SY is a non-standard GNU extension.");
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
				if (flags.W >= 3) man_warnx(".OP is a non-standard GNU extension.");

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
				type = tkTEXT;
				return buffer;
			}

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
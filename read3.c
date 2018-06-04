
#include <stdio.h>
#include <ctype.h>
#include <err.h>

#include "man.h"

static int width;
static int in;
static int ti;
static int nf;
static int PD;
static int line;
static int font;
static int prev_font;
static unsigned rm;

static char buffer[512];
static unsigned buffer_width;
static unsigned buffer_words;
static unsigned buffer_offset;
static unsigned buffer_flip_flop;


#define IN_DEFAULT 4

static void set_font(unsigned new_font) {
	if (new_font == FONT_P) new_font = prev_font;

	prev_font = font;
	if (font == new_font) return;
	// disable current font.
	switch(font) {
		case FONT_B:
		case FONT_I:
			break;
	}
	font = new_font;
	switch(font) {
		case FONT_B:
		case FONT_I:
			break;
	}
}

static void reset_font(void) {
	switch(font) {
		case FONT_B:
		case FONT_I:
			break;
	}
	font = prev_font = FONT_R;
}

static void flush(unsigned justify) {
	unsigned i;
	unsigned lm;
	int padding;
	int holes;

	if (!buffer_width) return;

	lm = ti >= 0 ? ti : in;
	ti = -1;

	// removing trailing whitespace.
	i = buffer_offset - 1;
	while(isspace(buffer[i])) { --i; --buffer_width; }
	buffer[++i] = 0;

	padding = width - buffer_width;
	holes = buffer_words - 1;
	if (padding <= 0) justify = 0;

	buffer_flip_flop = !buffer_flip_flop;
	padding += holes;

	// indent.
	for (i = 0; i < lm; ++i) fputc(' ', stdout);

	for (i = 0; ; ++i) {
		unsigned char c = buffer[i];
		if (c == 0) break;
		switch(c) {
			case ' ': case '\t':
				if (justify) {
					int j;
					int fudge;
					// taken from nroff.
					if (buffer_flip_flop) 
						fudge = padding / holes;
					else
						fudge = (padding - 1) / holes + 1;

					padding -= fudge;
					holes -= 1;
					//fudge++;
					for (j = 0; j < fudge; ++j) fputc(' ', stdout);


				} else fputc(' ', stdout);
				break;

			case FONT_R: case FONT_B: case FONT_I: case FONT_P:
				set_font(c);
				break;
			case ZWSPACE: break;
			case NBSPACE: fputc(' ', stdout); break;
			default: fputc(c, stdout);
		}
	}

	fputc('\n', stdout);
	buffer_width = 0;
	buffer_offset = 0;
	buffer_words = 0;
	buffer[0] = 0;
	width = rm - in;
	return;

}

/* append buffer */
static void append(const char *cp) {

	unsigned start, end;
	unsigned i, j, k;
	unsigned xlen;
	unsigned char c;

	xlen = 0;
	i = 0;
	j = 0;

	k = buffer_offset;

	for(;;) {
		while (isspace(cp[i])) ++i;
		start = i;
		xlen = 0;
		for(;;++i) {
			c = cp[i];
			if (c == 0 || isspace(c)) {
				end = i;
				/* xlen = printed size */
				/* if word ends w [.?!], should add 2 spaces...
				*/
				/* flush() depends on buffer_offset, etc */
				if (buffer_width + xlen > width) {
					buffer_offset = k;
					flush(1);
					k = 0;
				}
				
				if (xlen > width) {
					warnx("Word is too long.");
					continue;
				}
				for (j = start; j < end; ++j)
					buffer[k++] = cp[j];
				buffer[k++] = ' ';
				buffer[k] = 0;

				xlen++; /* space */
				if (j > 1) {
					/* if word ends w/ [.?!] should add extra space */
					unsigned char c = cp[j-1];
					if (c == '.' || c == '?' || c == '!') {
						xlen++;
						buffer[k++] = ' ';
						buffer[k] = 0;
					}
				}
				buffer_width += xlen;
				buffer_words++;

				if (c == 0) {
					buffer_offset = k;
					return;
				}
				break;

			} else {
				if (c <= NBSPACE) xlen++;
			}
		}
	}
}

static void set_indent(int new_in, int new_ti) {
	int lm;
	in = new_in;
	ti = new_ti;
	lm = new_ti >= 0 ? new_ti : new_in;
	width = rm - lm;
}

void man(FILE *fp) {


	in = 0;
	ti = -1;
	PD = 1;
	line = 1;
	nf = 0;

	font = FONT_R;
	prev_font = FONT_R;
	rm = 78;

	buffer_flip_flop = 0;
	buffer_width = 0;
	buffer_offset = 0;
	buffer_words = 0;
	buffer[0] = 0;
	set_indent(IN_DEFAULT, -1);

	read_init(fp);


	for(;;) {
		int x;
		const char *cp = read_text();
		if (type == tkEOF) {
			break;
		}


		switch(type) {
			case tkTH:
				break;
			case tkLP:
			case tkPP:
			case tkP:
				flush(0);
				reset_font();
				set_indent(IN_DEFAULT, -1);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				break;
			case tkIP:
				/* indented paragraph */
				flush(0);
				reset_font();
				set_indent(IN_DEFAULT, IN_DEFAULT + 4);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				break;

			case tkHP:
				/* hanging paragraph */
				flush(0);
				reset_font();
				set_indent(IN_DEFAULT + 4, IN_DEFAULT);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				break;

			case tkTP:
				/* tagged paragraph */
				flush(0);
				reset_font();
				set_indent(IN_DEFAULT + 4, IN_DEFAULT);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				break;

			case tkNF:
				flush(0);
				nf = 1;
				break;
			case tkFI:
				nf = 0;
				break;

			case tkIN:
				break;
			case tkPD:
				break;
			case tkRS:
			case tkRE:
				break;

			case tkSH:
			case tkSS:
				/* next-line */
				flush(0);
				reset_font();
				set_indent(IN_DEFAULT + 4, -1);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				break;

			case tkTEXT:
				if (nf) {
					int lm = ti >= 0 ? ti : in;
					ti = -1;
					for (x = 0; x < lm; ++x) fputc(' ', stdout);
					fprintf(stdout, "%.*s\n", width, cp);
				} else {
					// check for .SH / .SS trap!
					append(cp);
				}
				break;
		}


	}
	flush(0);
	// print footer...

	read_init(NULL);
}

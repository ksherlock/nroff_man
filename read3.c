
#include <stdio.h>
#include <ctype.h>
#include <err.h>

#include "man.h"

static int width;
static int in;
static int ti;
static int nf;
static int PD;
static int TP;
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
#define SS_DEFAULT 2

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


static int indent(int amt) {
	int i;
	if (amt == -1) {
		amt = ti >= 0 ? ti : in;
	}
	for (i = 0; i < amt; ++i) fputc(' ', stdout);
	return amt;
}

static void set_indent(int new_in, int new_ti) {
	int lm;
	in = new_in;
	ti = new_ti;
	lm = new_ti >= 0 ? new_ti : new_in;
	width = rm - lm;
}

static unsigned print(const char *cp, int fi) {
	unsigned i;
	unsigned char prev = 0;
	unsigned length = 0;
	for (i = 0; ;++i) {
		unsigned char c = cp[i];
		if (c == 0) return length;
		switch(c) {
			case FONT_R: case FONT_B: case FONT_I: case FONT_P:
				set_font(c);
				break;
			case ZWSPACE: break;
			case NBSPACE: fputc(' ', stdout); length++; break;
			case ' ': case '\t':
				if (fi) {
					c = ' ';
					if (prev != ' ') { fputc(' ', stdout); length++; }
				} else {
					fputc(c, stdout); length++;
				}
				break;
			default: fputc(c, stdout); length++;
		}
		prev = c;
	}
}

static unsigned xstrlen(const char *cp) {

	unsigned i;
	unsigned char prev = 0;
	unsigned length = 0;
	for (i = 0; ;++i) {
		unsigned char c = cp[i];
		if (c == 0) return length;
		switch(c) {
			case FONT_R: case FONT_B: case FONT_I: case FONT_P:
			case ZWSPACE:
				break;
			case NBSPACE: length++; break;
			case ' ': case '\t':
				c = ' ';
				if (prev != ' ') length++;
				break;
			default: length++;
		}
		prev = c;
	}
}

static void flush(unsigned justify) {
	unsigned i;
	int padding;
	int holes;

	if (!buffer_width) return;

	// removing trailing whitespace.
	i = buffer_offset - 1;
	while(isspace(buffer[i]) || buffer[i] == NBSPACE) { --i; --buffer_width; }
	buffer[++i] = 0;

	padding = width - buffer_width;
	holes = buffer_words - 1;
	if (padding <= 0) justify = 0;

	buffer_flip_flop = !buffer_flip_flop;
	padding += holes;

	// indent.
	indent(-1);
	set_indent(in, -1);

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

	fputc('\n', stdout); ++line;
	buffer_width = 0;
	buffer_offset = 0;
	buffer_words = 0;
	buffer[0] = 0;
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

				if (j > 1) {
					/* if word ends w/ [.?!] should add extra space */
					unsigned char c = cp[j-1];
					if (c == '.' || c == '?' || c == '!') {
						buffer[k++] = NBSPACE; /* stripped if end */
						xlen++;
					}
				}

				buffer[k++] = ' ';
				buffer[k] = 0;
				xlen++; /* space */
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


void man(FILE *fp) {


	unsigned trap = 0;
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
				TP = 4;
				set_indent(IN_DEFAULT + TP, IN_DEFAULT);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				trap = type;
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
				trap = 0;
				/* next-line */
				flush(0);
				reset_font();
				set_indent(IN_DEFAULT, -1);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				if (argc) {
					int i;
					if (type == tkSS) indent(SS_DEFAULT);
					set_font(FONT_B);
					for (i = 0; i < argc; ++i) {
						if (i) fputc(' ', stdout);
						print(argv[i], 1);
					}
					reset_font();
					fputc('\n', stdout); ++line;
				} else {
					trap = type;
				}
				break;

			case tkTEXT: {
				unsigned xline = line;
				if (nf) {
					indent(-1);
					set_indent(in, -1);
					print(cp, 0);
					fputc('\n', stdout); ++line;
					break;
				}
				if (!cp[0]) continue;
				switch(trap) {
					case tkSS:
						set_indent(IN_DEFAULT, SS_DEFAULT);
						set_font(FONT_B);
						break;

					case tkSH:
						set_indent(IN_DEFAULT, 0);
						set_font(FONT_B);
						break;
				}
				append(cp);
				switch(trap) {
					case tkSS:
					case tkSH:
						flush(0);
						reset_font();
						set_indent(IN_DEFAULT, -1);
						break;
					case tkTP:
						if (line > xline || buffer_width > in) {
							flush(0);
						} else {
							while (buffer_width < TP) {
								buffer[buffer_offset++] = NBSPACE;
								buffer_width++;
							}
							buffer[buffer_offset] = 0;
						}
						break;
				}
				trap = 0;
				break;
			}
		}


	}
	flush(0);
	// print footer...

	read_init(NULL);
}

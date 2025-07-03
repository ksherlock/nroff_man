

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>

#include <stdint.h>

#include "man.h"

extern char *italic_begin;
extern char *italic_end;
extern char *bold_begin;
extern char *bold_end;

/*
 * proper nroff uses .ll (line length) + .in/.ti to calc right margin.
 * This is backwards.
 */
extern unsigned width;
extern unsigned prev_in;
extern unsigned prev_ft;
extern unsigned prev_ll;
extern int line;


static unsigned char buffer[512];
static unsigned buffer_width;
static unsigned buffer_words;
static unsigned buffer_offset;
static unsigned buffer_flip_flop;
static unsigned prev_ftx = FONT_R;

void flush(unsigned justify);


void render_init(void) {
	buffer_flip_flop = 0;
	buffer_width = 0;
	buffer_offset = 0;
	buffer_words = 0;
	buffer[0] = 0;
	prev_ftx = FONT_R;
}



#define _(x) if (x && !flags.T) fputs(x, stdout); break

void set_font(unsigned new_font) {


	if (new_font == FONT_P) new_font = prev_ft;

	if (new_font == FONT_X) {
		prev_ftx = ft;
	} else {
		prev_ft = ft;
	}

	if (ft == new_font) return;
	/* disable current font. */
	switch(ft) {
		case FONT_B: _(bold_end);
		case FONT_I: _(italic_end);
		case FONT_X: prev_ft = prev_ftx; break;
	}
	ft = new_font;
	switch(ft) {
		case FONT_B: _(bold_begin);
		case FONT_I: _(italic_begin);
	}
}

void reset_font(void) {
	switch(ft) {
		case FONT_B: _(bold_end);
		case FONT_I: _(italic_end);
	}
	ft = prev_ft = prev_ftx = FONT_R;
}

/*
 * indent and remove temporary indent, if appropriate.
 */ 
static void indent(void) {
	int i, amt;

	if (ti >= 0) {
		amt = ti;
		ti = -1;
		width = ll - in;
	} else {
		amt = in;
	}

	if (!amt) return;

	switch(ft) {
		case FONT_B: _(bold_end);
		case FONT_I: _(italic_end);
	}
	for (i = 0; i < amt; ++i) fputc(' ', stdout);
	switch(ft) {
		case FONT_B: _(bold_begin);
		case FONT_I: _(italic_begin);
	}
}
#undef _

void set_ti(int new_ti) {
	int lm;
	ti = new_ti;
	lm = new_ti < 0 ? in : new_ti;
	width = ll - lm;
}

void set_in(int new_in) {
	prev_in = in;
	in = new_in;
	if (ti < 0) width = ll - new_in;
}

void set_in_ti(int new_in, int new_ti) {
	int lm;
	prev_in = in;
	in = new_in;
	ti = new_ti;
	lm = new_ti >= 0 ? new_ti : new_in;
	width = ll - lm;
}

void set_ll(int new_ll) {
	int lm;
	prev_ll = ll;
	ll = new_ll;
	lm = ti >= 0 ? ti : in;
	width = ll - lm;
}



unsigned print(const unsigned char *cp, int fi) {
	unsigned i;
	uint_fast8_t prev = 0;
	unsigned length = 0;

	indent();
	for (i = 0; ;++i) {
		uint_fast8_t c = cp[i];
		if (c == 0) return length;
		switch(c) {
			case FONT_R: case FONT_B: case FONT_I: case FONT_P: case FONT_X:
				set_font(c);
				break;
			case HYPHEN: case ZWSPACE: break;
			case NBSPACE: case XSPACE: fputc(' ', stdout); length++; break;
			case ' ': case '\t':
				if (fi) {
					c = ' ';
					if (prev != ' ') { fputc(' ', stdout); length++; }
				} else {
					fputc(c, stdout); length++;
				}
				break;
			default: 
				if (flags.T == FMT_ASCII) {
					if (ft == FONT_B) {
						fputc(c, stdout);
						fputc(8, stdout);
					}
					if (ft == FONT_I) {
						fputs("_\x08", stdout);
					}
				}
				fputc(c, stdout); length++;
				break;
		}
		prev = c;
	}
}



unsigned xstrlen(const unsigned char *cp) {
	unsigned length = 0;
	unsigned i;
	for (i = 0; ; ++i) {
		uint_fast8_t c = cp[i];
		if (!c) return length;
		if (c <= NBSPACE) length++;
	}
}

unsigned xstrnlen(const unsigned char *cp, unsigned max) {

	unsigned length = 0;
	unsigned i;
	for (i = 0; i < max; ++i) {
		uint_fast8_t c = cp[i];
		if (!c) return length;
		if (c <= NBSPACE) length++;
	}
	return length;
}

static unsigned is_sentence(const unsigned char *cp, unsigned offset) {
	uint_fast8_t c;
	if (offset == 0) return 0;
	--offset;
	while (offset) {
		c = cp[offset];
		if (c == ')' || c == ']' || c == '\'' || c == '"' || c == '*'
			|| c == FONT_R || c == FONT_B || c == FONT_I || c == FONT_P) {
			--offset;
			continue;
		}
		break;
	}
	c = cp[offset];
	if (c == '.' || c == '?' || c == '!') return 1;
	return 0;
}




static int hyphenate(const unsigned char *in, unsigned available, int *rv) {
	/* check for hyphens in the text... */
	/* todo: - hyphenate with - character, if word is [A-Za-z-]+ */
	uint_fast8_t c;
	unsigned i;
	int hpos = -1;
	unsigned hlen = 0;
	unsigned xlen = 0;

	for (i = 0; ; ) {
		c = in[i++];
		switch(c) {
		case HYPHEN: hpos = i; hlen = xlen; break;
		case ' ': case '\t': case 0:
				if (hlen) {
					rv[0] = hpos;
					rv[1] = hlen;
					return 1;
				}
				return 0;
		default:
			if (c <= NBSPACE) {
				xlen++;
				if (xlen >= available) {
					if (hlen) {
						rv[0] = hpos;
						rv[1] = hlen;
						return 1;
					}
					return 0;
				}
			}
			break;
		}
	}
	return -1;
}


void append_font(unsigned font) {
	/* todo - check if buffer is overflowing */

	/* if there is trailing WS, this inserts a FONT_X token before the whitespace. */
	unsigned k;
	unsigned xfont = 0;

	k = buffer_offset;
	if (k == 0) {
		buffer[0] = font;
		buffer_offset++;
		return;
	}

	while (k > 0) {
		uint_fast8_t c = buffer[--k];
		if (c == ' ' || c == XSPACE) {
			buffer[k+1] = c;
			xfont = 1;
		} else break;
	}
	if (xfont) {
		buffer[k+1] = FONT_X;
		++buffer_offset;
	}
	buffer[buffer_offset++] = font;
}
/* append buffer */
void append(const unsigned char *cp) {

	unsigned start, end;
	unsigned i, j, k;
	unsigned xlen;
	uint_fast8_t c;

	xlen = 0;
	i = 0;
	j = 0;

	if (cp[0] == ' ') flush(0);

	k = buffer_offset;

	for(;;) {
		while (isspace(cp[i])) ++i;
		if (cp[i] == 0) { buffer_offset = k; return; }
		start = i;
		xlen = 0;
		for(;;++i) {
			c = cp[i];
			if (c == 0 || isspace(c)) {
				int avail;
				end = i;
				/* xlen = printed size */
				/* if word ends w [.?!], should add 2 spaces...
				*/
				/* flush() depends on buffer_offset, etc */
				avail = width - buffer_width;

				/* todo -- also check if k + end - start + 1 > sizeof(buffer) */

				if (buffer_width + xlen > width) {

					/* try to hyphenate */
					if (hy && avail > 2) {
						int cookie[2];
						int ok;
						ok = hyphenate(cp + start, avail, cookie);
						if (ok) {
							unsigned xend;
							xend = start + cookie[0];

							for (j = start; j < xend; ++j)
								buffer[k++] = cp[j];
							buffer[k++] = '-';
							buffer_width += cookie[1] + 1;
							buffer_words++;

							xlen -= cookie[1];
							start = xend;
						}
					}

					buffer_offset = k;
					buffer[k] = 0;
					flush(1);
					k = 0;
				}
				
				if (xlen > width) {
					man_warnx("Word is too long.");
					xlen = 0;
					continue;
				}
				for (j = start; j < end; ++j)
					buffer[k++] = cp[j];

				if (c == 0 && j > 1) {
					/* if word ends w/ [.?!] should add extra space */
					if (is_sentence(cp, j)) {
						buffer[k++] = ' ';
						xlen++;
					}
				}

				buffer[k++] = XSPACE;
				buffer[k] = 0;
				xlen++; /* space */
				buffer_width += xlen;
				buffer_words++;

				if (c == 0) {
					buffer_offset = k;
					return;
				}
				xlen = 0;
				break;

			} else {
				if (c <= NBSPACE) xlen++;
			}
		}
	}
}

void set_tag(const unsigned char *cp, unsigned IP) {
	/* set the tag for IP/TP */
	unsigned n;

	/* assumes buffer has been flushed */
	/* set_in_ti(LM + IP, LM); */

	n = xstrlen(cp);
	if (n >= width || n >= IP) { 
		append(cp);
		flush(0);
	} else {
		uint_fast8_t c;
		unsigned i;
		for (i = 0; ; ++i) {
			c = cp[i];
			if (c == 0) break;
			if (c == ' ' || c == '\t') c = NBSPACE;
			buffer[buffer_offset++] = c;
			if (c <= NBSPACE) buffer_width++;
		}
		buffer[buffer_offset] = 0;

		while(buffer_width < IP) {
			buffer[buffer_offset++] = NBSPACE;
			buffer_width++;
		}
		buffer[buffer_offset] = 0;
	}
}


void flush(unsigned justify) {
	unsigned i;
	int padding;
	int holes;


	/* removing trailing whitespace. */
	i = buffer_offset;
	if (!i) return;
	while (i) {
		uint_fast8_t c;
		c = buffer[i-1];
		if (c == ' ' || c == XSPACE ){ --i; --buffer_width; }
		else break;
	}
	if (!i) goto exit;
	buffer[i] = 0;


	buffer_flip_flop = !buffer_flip_flop;
	padding = width - buffer_width;
	holes = buffer_words - 1;
	if (padding <= 0 || holes <= 0) justify = 0;


	if (na || ad == 'l') {
		print(buffer, 0);
		goto exit;
	}
	if (ad == 'r') {
		while(padding--) fputc(' ', stdout);
		print(buffer, 0);
		goto exit;
	}
	if (ad == 'c') {
		/* if (buffer_flip_flop) ++padding; */
		padding >>= 1;
		while(padding--) fputc(' ', stdout);
		print(buffer, 0);
		goto exit;
	}

	/* indent. */
	indent();
	padding += holes;

	for (i = 0; ; ++i) {
		uint_fast8_t c = buffer[i];
		if (c == 0) break;
		switch(c) {
			case XSPACE:
				if (justify) {
					int j;
					int fudge;
					/* taken from rosenkrantz nroff
					 * which was taken from Software Tools
					 * which was taken from Doug McIlroy's BCPL roff.
					 */
					if (buffer_flip_flop) 
						fudge = padding / holes;
					else
						fudge = (padding - 1) / holes + 1;

					padding -= fudge;
					holes -= 1;
					for (j = 0; j < fudge; ++j) fputc(' ', stdout);


				} else fputc(' ', stdout);
				break;

			case FONT_R: case FONT_B: case FONT_I: case FONT_P: case FONT_X:
				set_font(c);
				break;
			case HYPHEN: case ZWSPACE: break;
			case NBSPACE: fputc(' ', stdout); break;
			default:
				if (flags.T == FMT_ASCII) {
					if (ft == FONT_B) {
						fputc(c, stdout);
						fputc(8, stdout);
					}
					if (ft == FONT_I) {
						fputs("_\x08", stdout);
					}
				}
				fputc(c, stdout);
				break;
		}
	}


exit:
	fputc('\n', stdout); ++line;

	buffer_width = 0;
	buffer_offset = 0;
	buffer_words = 0;
	buffer[0] = 0;
	return;

}

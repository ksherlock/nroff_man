
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include "man.h"

static int width;
static int in;
static int ti;
static int nf;
static int PD;
static int TP;
static int IP;
static int line;
static int font;
static int prev_font;
static unsigned rm;

static char buffer[512];
static unsigned buffer_width;
static unsigned buffer_words;
static unsigned buffer_offset;
static unsigned buffer_flip_flop;


#define IN_DEFAULT 7
#define SS_DEFAULT 3

static char *bold_begin = NULL;
static char *bold_end = NULL;
static char *italic_begin = NULL;
static char *italic_end = NULL;

static char tcap_buffer[1024];
static unsigned tcap_buffer_offset = 0;

static int tputs_helper(int c) {
	if (c) tcap_buffer[tcap_buffer_offset++] = c;
	return 1;
}

void man_init(void) {
	static char buffer[1024];
	static char buffer2[1024];
	char *term;
	char *cp;
	char *pcap;
	int ok;

	term = getenv("TERM");
	if (!term || !*term) {
		warnx("TERM undefined."); return;
	}

	ok = tgetent(buffer, term);
	if (ok < 0) {
		warnx("No termcap for %s", term);
		return;
	}

	tcap_buffer_offset = 0;

	pcap = buffer2;
	cp = tgetstr("us", &pcap);
	if (cp) {
		italic_begin = tcap_buffer + tcap_buffer_offset;
		tputs(cp, 0, tputs_helper);
		tcap_buffer[tcap_buffer_offset++] = 0;
	}
	cp = tgetstr("ue", &pcap);
	if (cp) {
		italic_end = tcap_buffer + tcap_buffer_offset;
		tputs(cp, 0, tputs_helper);
		tcap_buffer[tcap_buffer_offset++] = 0;
	}
	cp = tgetstr("so", &pcap);
	if (cp) {
		bold_begin = tcap_buffer + tcap_buffer_offset;
		tputs(cp, 0, tputs_helper);
		tcap_buffer[tcap_buffer_offset++] = 0;
	}
	cp = tgetstr("se", &pcap);
	if (cp) {
		bold_end = tcap_buffer + tcap_buffer_offset;
		tputs(cp, 0, tputs_helper);
		tcap_buffer[tcap_buffer_offset++] = 0;
	}

}

static void set_font(unsigned new_font) {
	if (new_font == FONT_P) new_font = prev_font;

	prev_font = font;
	if (font == new_font) return;
	// disable current font.
	switch(font) {
		case FONT_B:
			if (bold_end) fputs(bold_end, stdout);
			break;
		case FONT_I:
			if (italic_end) fputs(italic_end, stdout);
			break;
	}
	font = new_font;
	switch(font) {
		case FONT_B:
			if (bold_begin) fputs(bold_begin, stdout);
			break;
		case FONT_I:
				if (italic_begin) fputs(italic_begin, stdout);
			break;
	}
}

static void reset_font(void) {
	switch(font) {
		case FONT_B:
			if (bold_end) fputs(bold_end, stdout);
			break;
		case FONT_I:
			if (italic_end) fputs(italic_end, stdout);
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

static void pd(void) {
	unsigned i;
	for (i = 0; i < PD; ++i) {
		fputc('\n', stdout);
		++line;
	}
}

static void newline(void) {
	fputc('\n', stdout);
	++line;
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
	if (padding <= 0 || holes <= 0) justify = 0;

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

static char header[80];
static char footer[80];
static char *th_source = NULL;

static void at(void) {
	char *cp = NULL;
	if (argc > 0) {
		char *arg = argv[0];
		switch(arg[0]) {
			/* case '3': break; */
			case '4': cp = "System III"; break;
			case '5': cp = argc > 1 ? "System V Release 2" : "System V Release";
				break;
		}
	}
	if (!cp) cp = "7th Edition";
	if (th_source) free(th_source);
	th_source = strdup(cp);
}

static void uc(void) {
	char *cp = NULL;
	if (argc > 0) {
		char *arg = argv[0];
		switch(arg[0]) {
			/* case '3': break; */
			case '4': cp = "4th Berkeley Distribution"; break;
			case '5': cp = "4.2 Berkeley Distribution"; break;
			case '6': cp = "4.3 Berkeley Distribution"; break;
			case '7': cp = "4.4 Berkeley Distribution"; break;
		}
	}
	if (!cp) cp = "3rd Berkeley Distribution";
	if (th_source) free(th_source);
	th_source = strdup(cp);	
}

static void th(void) {
	/* .TH title section date [source [volume]] */

	/* header */
	/* #{title}(#{section}) - #{volume} - #{title}(#{section}) */
	/* footer */
	/* #{source} - #{date} - #{title}(#{section}) */
	/* GNO man uses Page # right footer */

	char *title = NULL;
	char *section = NULL;
	char *date = NULL;
	char *source = NULL;
	char *volume = NULL;

	unsigned i, j, l;
	char c;


	if (argc >= 1) title = argv[0];
	if (argc >= 2) section = argv[1];
	if (argc >= 3) date = argv[2];
	if (argc >= 4) source = argv[3];
	if (argc >= 5) volume = argv[4];

	header[0] = 0;
	footer[0] = 0;

	if (source) {
		if (th_source) free(th_source);
		th_source = strdup(source);
	}


	#define _(x) if (!section[1]) volume = x; break
	if (!volume && section) {
		switch(section[0]) {
			case '1': _("General Commands Manual");
			case '2': _("System Calls Manual");
			case '3': _("Library Functions Manual");
			case '4': _("Device Drivers Manual");
			case '5': _("File Formats Manual");
			case '6': _("Games Manual");
			case '7': _("Miscellaneous Information Manual");
			case '8': _("System Manager\'s Manual");
			case '9': _("Kernel Developer\'s Manual");
			default:
				break;
		}
	}
	#undef x

	if (!title) title = "";
	if (!section) section = "";
	if (!date) date = "";
	if (!volume) volume = "";

	memset(header, ' ', 78);	
	memset(footer, ' ', 78);
	header[78] = 0;	
	footer[78] = 0;	

	// todo - truncate if strlen(title) * 2 + strlen(section) * 2 + 4 + strlen()

	j = 0;
	for(i = 0; ; ++i) { char c = title[i]; if (!c) break; header[j++] = c; }
	header[j++] = '(';
	for(i = 0; ; ++i) { char c = section[i]; if (!c) break; header[j++] = c; }
	header[j++] = ')';
	// mirror at the end of the string.
	for (i = 0; i < j; ++i) {
		char c = header[i];
		header[78 - j + i] = c;
		footer[78 - j + i] = c;
	}

	l = strlen(volume);
	j = (78 - l) >> 1;
	for (i = 0; ; ++i) { char c = volume[i]; if (!c) break; header[j++] = c; }

	l = strlen(date);
	j = (78 - l) >> 1;
	for (i = 0; ; ++i) { char c = date[i]; if (!c) break; footer[j++] = c; }


	fputs(header, stdout);
	fputs("\n\n\n", stdout);
	line += 3;
}

static void print_footer(void) {

	unsigned i;
	char c;

	if (!header[0]) return;

	if (th_source) {
		i = 0;
		while ((c = th_source[i])) footer[i++] = c;
	}

	fputs("\n\n\n", stdout); line += 3;
	fputs(footer, stdout);
	fputc('\n', stdout); ++line;

	if (th_source) free(th_source);
	th_source = NULL;
}

void man(FILE *fp) {


	unsigned trap = 0;
	in = 0;
	ti = -1;
	PD = 1;
	IP = 7;
	TP = 7;
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

	th_source = NULL;
	header[0] = 0;
	footer[0] = 0;

	set_indent(IN_DEFAULT, -1);

	read_init(fp);


	for(;;) {
		int x;
		const char *cp = read_text();
		if (type == tkEOF) {
			break;
		}
		if (type != tkTEXT) trap = 0;

		switch(type) {
			case tkAT: at(); break;
			case tkUC: uc(); break;
			case tkTH: th(); break;

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
				/* IP [tag [width]] */
				flush(0);
				reset_font();
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				if (argc >= 2) {
					/* set IP width... */
				}

				if (argc >= 1) {
					set_indent(IN_DEFAULT + IP, IN_DEFAULT);
					append(argv[0]);
					/* same logic as TP trap */
					if (buffer_width >= IP) flush(0);
					else {
						unsigned i;
						for (i = 0; i < buffer_offset; ++i)
							if (isspace(buffer[i])) buffer[i] = NBSPACE;
						for(i = buffer_width; i < IP; ++i) {
							buffer[buffer_offset++] = NBSPACE;
							buffer_width++;
						}
						buffer[buffer_offset] = 0;
						buffer_words--;
					}
				} else {
					set_indent(IN_DEFAULT + IP, -1);
				}
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
					//set_font(FONT_B);
					for (i = 0; i < argc; ++i) {
						if (i) fputc(' ', stdout);
						print(argv[i], 1);
					}
					//reset_font();
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
						//set_font(FONT_B);
						break;

					case tkSH:
						set_indent(IN_DEFAULT, 0);
						//set_font(FONT_B);
						break;
				}
				append(cp);
				switch(trap) {
					case tkSS:
					case tkSH:
						flush(0);
						//reset_font();
						set_indent(IN_DEFAULT, -1);
						break;
					case tkTP:
						if (buffer_width >= TP) flush(0);
						else {
							unsigned i;
							for (i = 0; i < buffer_offset; ++i)
								if (isspace(buffer[i])) buffer[i] = NBSPACE;
							for (i = buffer_width; i < TP; ++i) {
								buffer[buffer_offset++] = NBSPACE;
								buffer_width++;
							}
							buffer_words--;
							buffer[buffer_offset] = 0;			
						}

				}
				trap = 0;
				break;
			}
		}


	}
	flush(0);
	// print footer...
	print_footer();

	read_init(NULL);
}

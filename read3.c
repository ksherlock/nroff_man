
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include <errno.h>
#include <assert.h>
#include "man.h"

/* inter-paragraph spacing */
static int PD;
/* paragraph indent for HP, IP, TP. reset by SH/SS */
static int IP;
/*static int RS;*/
/* left margin. initially 0, set to 7 by SH/SS. updated by RE/RS */
static int LM;
/* right margin. always 78 */
static unsigned RM;

/* width. RM - (LM + ti/in) */
static int width;
/* current indent level */
static int in;
/* temporary indent level */
static int ti;
/* non-format flag */


static int line;
static int font;
static int prev_font;

static char buffer[512];
static unsigned buffer_width;
static unsigned buffer_words;
static unsigned buffer_offset;
static unsigned buffer_flip_flop;

static int rs_count;
static int rs_stack[8];



#define PP_INDENT 7
#define SS_INDENT 3
#define SH_INDENT 0

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
	/* disable current font. */
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

/*
 * indent and remove temporary indent, if appropriate.
 */ 
static void indent(void) {
	int i, amt;

	if (ti >= 0) {
		amt = ti;
		ti = -1;
		width = RM - in;
	} else {
		amt = in;
	}

	if (!amt) return;

	#define _(x) if (x) fputs(x, stdout); break;
	switch(font) {
		case FONT_B: _(bold_end);
		case FONT_I: _(italic_end);
	}
	for (i = 0; i < amt; ++i) fputc(' ', stdout);
	switch(font) {
		case FONT_B: _(bold_begin);
		case FONT_I: _(italic_begin);
	}
	#undef _
}

static void set_indent(int new_in, int new_ti) {
	int lm;
	in = new_in;
	ti = new_ti;
	lm = new_ti >= 0 ? new_ti : new_in;
	width = RM - lm;
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
			case NBSPACE: case XSPACE: fputc(' ', stdout); length++; break;
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


static void flush(unsigned justify) {
	unsigned i;
	int padding;
	int holes;


	/* removing trailing whitespace. */
	i = buffer_offset;
	if (!i) return;
	while (i) {
		unsigned char c;
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


	/* indent. */
	indent();


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

	padding += holes;

	for (i = 0; ; ++i) {
		unsigned char c = buffer[i];
		if (c == 0) break;
		switch(c) {
			case XSPACE:
				if (justify) {
					int j;
					int fudge;
					/* taken from nroff. */
					if (buffer_flip_flop) 
						fudge = padding / holes;
					else
						fudge = (padding - 1) / holes + 1;

					padding -= fudge;
					holes -= 1;
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


exit:
	fputc('\n', stdout); ++line;
exit2:
	buffer_width = 0;
	buffer_offset = 0;
	buffer_words = 0;
	buffer[0] = 0;
	return;

}

static unsigned is_sentence(const char *cp, unsigned offset) {
	char c;
	if (offset == 0) return 0;
	--offset;
	while (offset) {
		c = cp[offset];
		if (c == ')' || c == ']' || c == '\'' || c == '"' || c == '*') {
			--offset;
			continue;
		}
		break;
	}
	c = cp[offset];
	if (c == '.' || c == '?' || c == '!') return 1;
	return 0;
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
				end = i;
				/* xlen = printed size */
				/* if word ends w [.?!], should add 2 spaces...
				*/
				/* flush() depends on buffer_offset, etc */
				if (buffer_width + xlen > width) {
					buffer_offset = k;
					buffer[k] = 0;
					flush(1);
					k = 0;
				}
				
				if (xlen > width) {
					warnx("Word is too long.");
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
				break;

			} else {
				if (c <= NBSPACE) xlen++;
			}
		}
	}
}

static int xstrlen(const char *cp) {
	int length = 0;
	unsigned i;
	for (i = 0; ; ++i) {
		unsigned char c = cp[i];
		if (!c) return length;
		if (c <= NBSPACE) length++;
	}
}


static void set_tag(const char *cp) {
	/* set the tag for IP/TP */
	int n;
	int xline = line;

	/* assumes buffer has been flushed */
	/* set_indent(LM + IP, LM); */

	n = xstrlen(cp);
	if (n >= width || n >= IP) { 
		append(cp);
		flush(0);
	} else {
		unsigned char c;
		unsigned j = 0;
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



static char header[80];
static char footer[80];
static char *th_source = NULL;

/* read an int, which is expected to be in the range 0-9. */
static int get_int(const char *arg) {
	if (!arg || !arg[0]) return -1;
	if (isdigit(arg[0]) && !arg[1]) return arg[0] - '0';
	return -1;
}

static int get_unit(const char *arg, int dv) {
	double d;
	char sign = 0;
	char c;
	char *cp = NULL;
	
	if (!arg || !arg[0]) return dv;
	c = arg[0];
	if (c == '+' || c == '-') {
		sign = c;
		++arg;
	}
	c = arg[0];
	if (!isdigit(c) && c != '.') return dv;

	errno = 0;
	d = strtod(arg, &cp);
	if (errno) return dv;

	c = *cp;
	switch (c) {
		case 'c': /* centimeter */
			d *= 94.5 / 24.0;
			break;
		case 'i': /* inch */
			d *= 10.0; /* 240.0 / 24.0; */
			break;
		case 'P': /* pica - 1/6 inch */
			d *= 10.0 / 6.0;
			break;
		case 'p': /* point - 1/72 inch */
			d *= 10.0 / 72.0;
			break;
		case 'f':
			d *= 65536.0 / 24.0;
			break;
		case 'M': /* mini em - 1/100 em */
			d  /= 100.0;
			break;

		case 'm':
		case 'n':
		case 'u':
		case 'v':
		default:
			break;
	}
	return (int)d;
}

static void at(void) {
	char *cp = NULL;

	switch(get_int(argv[0])) {
		default:
		case 3: cp = "7th Edition"; break;
		case 4: cp = "System III"; break;
		case 5:
			if (argc == 2 && get_int(argv[1]) == 2)
				cp = "System V Release 2";
			else cp = "System V Release";
	}
	if (th_source) free(th_source);
	th_source = strdup(cp);
}

static void uc(void) {
	char *cp = NULL;
	switch (get_int(argv[0])) {
		default:
		case 3: cp = "3rd Berkeley Distribution"; break;
		case 4: cp = "4th Berkeley Distribution"; break;
		case 5: cp = "4.2 Berkeley Distribution"; break;
		case 6: cp = "4.3 Berkeley Distribution"; break;
		case 7: cp = "4.4 Berkeley Distribution"; break;
	}
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

	const char *title = NULL;
	const char *section = NULL;
	const char *date = NULL;
	const char *source = NULL;
	const char *volume = NULL;

	unsigned i, j, l;


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


	#define _(x) volume = x; break
	if (!volume) {
		switch(get_int(section)) {
			case 1: _("General Commands Manual");
			case 2: _("System Calls Manual");
			case 3: _("Library Functions Manual");
			case 4: _("Device Drivers Manual");
			case 5: _("File Formats Manual");
			case 6: _("Games Manual");
			case 7: _("Miscellaneous Information Manual");
			case 8: _("System Manager\'s Manual");
			case 9: _("Kernel Developer\'s Manual");
			default: _("");
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

	/*
	 todo - truncate if strlen(title) * 2 + strlen(section) * 2 + 4 + strlen()
	*/
	j = 0;
	for(i = 0; ; ++i) { char c = title[i]; if (!c) break; header[j++] = c; }
	header[j++] = '(';
	for(i = 0; ; ++i) { char c = section[i]; if (!c) break; header[j++] = c; }
	header[j++] = ')';
	/* mirror at the end of the string. */
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
	const char *cp = NULL;
	char c;

	if (!header[0]) return;

	cp = th_source ? th_source : flags.os;
	if (cp) {
		i = 0;
		while ((c = cp[i])) footer[i++] = c;
	}

	fputs("\n\n\n", stdout); line += 3;
	fputs(footer, stdout);
	fputc('\n', stdout); ++line;

	if (th_source) free(th_source);
	th_source = NULL;
}

void man(FILE *fp, const char *filename) {


	unsigned trap = 0;


	in = 0;
	ti = -1;
	PD = 1;
	IP = PP_INDENT;
	line = 1;

	fi = 1;
	ad = 'b';
	na = 0;

	font = FONT_R;
	prev_font = FONT_R;
	RM = 78;
	LM = PP_INDENT;

	buffer_flip_flop = 0;
	buffer_width = 0;
	buffer_offset = 0;
	buffer_words = 0;
	buffer[0] = 0;

	th_source = NULL;
	header[0] = 0;
	footer[0] = 0;


	rs_count = 0;

	/* indent isn't set until .SH, .SS, .PP, etc */
	set_indent(0, -1);

	read_init(fp, filename);


	for(;;) {
		int x;
		const char *cp = read_text();
		if (type == tkEOF) {
			break;
		}

		switch(type) {
			case tkAT: at(); break;
			case tkUC: uc(); break;
			case tkTH: th(); break;

			case tkLP:
			case tkPP:
			case tkP:
				trap = 0;
				flush(0);
				reset_font();
				set_indent(LM, -1);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				break;


			case tkIP:
				/* indented paragraph */
				/* IP [tag [width]] */
				trap = 0;
				flush(0);
				reset_font();
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				if (argc >= 2) {
					/* set IP width... */
					IP = get_unit(argv[1], PP_INDENT);
				}

				if (argc >= 1) {
					set_indent(LM + IP, LM);
					set_tag(argv[0]);
				} else {
					set_indent(LM + IP, -1);
				}
				break;

			case tkHP:
				/* hanging paragraph */
				/* .HP [indent] */
				trap = 0;
				flush(0);
				reset_font();
				if (argc >= 1) {
					/* set IP width... */
					IP = get_unit(argv[0], PP_INDENT);
				}
				set_indent(LM + IP, LM);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				break;

			case tkTP:
			case tkTQ:
				/* tagged paragraph */
				/*
					.TP [indent]
					tag text
					...

					.TQ is a GNU extension; it is essentially .TP
					without the PD padding.
				*/
				trap = 0;
				flush(0);
				reset_font();
				if (argc >= 1) {
					/* set IP width... */
					IP = get_unit(argv[0], PP_INDENT);
				}
				set_indent(LM + IP, LM);
				if (type == tkTP)
					for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				trap = tkTP;
				break;

			case tkSY:
				/*
					GNU extension
					.SY xxx -> .HP width(xxx) \ \fBxxx\fR
				*/
				trap = 0;
				flush(0);
				if (argc) {
					IP = xstrlen(argv[0]) + 1;					
				}
				set_indent(LM + IP, LM);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				set_tag(argv[0]);
				break;
			case tkYS:
				flush(0);
				break;

			case tknf:
			case tkEX:
				trap = 0;
				flush(0);
				fi = 0;
				break;
			case tkfi:
			case tkEE:
				trap = 0;
				fi = 1;
				break;

			case tkin:
				break;

			case tkbr:
				trap = 0;
				flush(0);
				break;
			case tksp: {
				int n;
				trap = 0;
				flush(0);
				n = get_unit(argv[0], 1);
				while (--n >= 0 ) {
					fputc('\n', stdout); ++line;
				}
				break;
			}

			case tkPD:
				PD = get_unit(argv[0], 1);
				break;
			case tkRS:
				/* RS [width] */
				flush(0);
				rs_stack[rs_count++] = LM;
				if (argc >= 1) IP = get_unit(argv[0], PP_INDENT);
				LM += IP;
				set_indent(LM, ti);
				break;
			case tkRE:
				/* RE [level] */
				/* RE - pop last RS */
				/* RE 1 - pop all RS */
				/* RE 2 - pop all but 1 RS */
				/* etc. */
				flush(0);
				if (argc) {
					int n = get_int(argv[0]);
					if (n < 1) n = 1;
					--n;
					if (n < rs_count) {
						LM = rs_stack[n];
						rs_count = n;
					} else {
						warnx("Invalid RS count (%d / %d)", n + 1, rs_count);
					}

				} else {
					if (rs_count)
						LM = rs_stack[--rs_count];
					else LM = 0;
				}

				set_indent(LM, ti);
				break;

			case tkSH:
			case tkSS:
				/* next-line scoped */
				trap = 0;
				flush(0);
				reset_font();

				na = 0;
				ad = 'b';
				fi = 1;

				LM = PP_INDENT;
				IP = PP_INDENT;
				if (type == tkSH && argc == 1 && !strcmp(argv[0], "SYNOPSIS")) {
					na = 1;
				}
				set_indent(PP_INDENT, type == tkSS ? SS_INDENT : SH_INDENT);
				for (x = 0; x < PD; ++x) { fputc('\n', stdout); ++line; }
				if (argc) {
					int i;
					for (i = 0; i < argc; ++i) append(argv[i]);
					flush(0);
				} else {
					trap = type;
				}
				break;

			case tkTEXT:
				if (!fi) {
					indent();
					print(cp, 0);
					fputc('\n', stdout); ++line;
					break;
				}
				if (trap == tkTP) {
					trap = 0;
					set_tag(cp);
					cp = NULL;
				}
				if (!cp || !cp[0]) continue;
				append(cp);
				switch(trap) {
					case tkSS:
					case tkSH:
						flush(0);
						/* reset_font(); */
						/* set_indent(PP_INDENT, -1); */
						break;
				}
				trap = 0;
				break;
		}
	}
	flush(0);
	reset_font();
	print_footer();
	read_init(NULL, NULL);
}

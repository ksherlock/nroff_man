
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

/* inter-paragraph spacing */
static int PD;
/* paragraph indent for HP, IP, TP. reset by SH/SS */
static int IP;
/*static int RS;*/
/* left margin. initially 0, set to 7 by SH/SS. updated by RE/RS */
static int LM;
/* right margin. always 78 */
static unsigned RM;


#define br_PD() if (PD && !ns) { line += PD; for(x = 0; x < PD; ++x) fputc('\n', stdout); }


extern int line;
extern unsigned prev_in;

extern void set_ti(int);
extern void set_in(int);
extern void set_in_ti(int, int);
extern void flush(unsigned justify);
extern void set_tag(const unsigned char *cp, unsigned IP);
extern unsigned print(const unsigned char *cp, int fi);
extern void append(const unsigned char *cp);
extern void set_font(unsigned new_font);
extern unsigned xstrlen(const unsigned char *cp);


static int rs_count;
static int rs_stack[8];



#define PP_INDENT 7
#define SS_INDENT 3
#define SH_INDENT 0

char *bold_begin = NULL;
char *bold_end = NULL;
char *italic_begin = NULL;
char *italic_end = NULL;

static char tcap_buffer[1024];
static unsigned tcap_buffer_offset = 0;

static int tputs_helper(int c) {
	if (c) tcap_buffer[tcap_buffer_offset++] = c;
	return 1;
}

void tc_init(void) {
	static char buffer[1024];
	static char buffer2[1024];
	const char *term;
	char *cp;
	char *pcap;
	int ok;

	term = flags.t ? flags.t : getenv("TERM");
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
	if (!flags.i) {
		/* ZH/ZR are italic begin/end.  Not usually supported. */

		cp = tgetstr("ZH", &pcap);
		if (cp) {
			italic_begin = tcap_buffer + tcap_buffer_offset;
			tputs(cp, 0, tputs_helper);
			tcap_buffer[tcap_buffer_offset++] = 0;
		}
		cp = tgetstr("ZR", &pcap);
		if (cp) {
			italic_end = tcap_buffer + tcap_buffer_offset;
			tputs(cp, 0, tputs_helper);
			tcap_buffer[tcap_buffer_offset++] = 0;
		}

		if (!italic_begin) {
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
		}
	}

	if (!flags.b) {
		cp = tgetstr("md", &pcap);
		if (cp) {
			bold_begin = tcap_buffer + tcap_buffer_offset;
			tputs(cp, 0, tputs_helper);
			tcap_buffer[tcap_buffer_offset++] = 0;
		}
		cp = tgetstr("me", &pcap);
		if (cp) {
			bold_end = tcap_buffer + tcap_buffer_offset;
			tputs(cp, 0, tputs_helper);
			tcap_buffer[tcap_buffer_offset++] = 0;
		}

		if (!bold_begin) {
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
	}
}










static char header[80];
static char footer[80];
static char *th_source = NULL;

/* read an int, which is expected to be in the range 0-9. */
int get_int(const unsigned char *arg) {
	if (!arg || !arg[0]) return -1;
	if (isdigit(arg[0]) && !arg[1]) return arg[0] - '0';
	return -1;
}

int get_unit(const unsigned char *arg, int dv, int base) {
	double d;
	char sign = 0;
	uint_fast8_t c;
	char *cp = NULL;
	int rv;
	unsigned i;
	
	if (!arg) return dv;

	i = 0;
	while (arg[i] == ' ') ++i;
	arg += i;
	c = arg[0];
	if (!c) return dv;

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
			break;
		default:
			break;
	}

	rv = (int)d;
	if (sign && base >= 0) {
		if (sign == '+') base += rv;
		else base -= rv;
		if (base < 0) return 0;
		return base;
	}
	if (sign == '-') rv = 0;

	return rv;
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

	const unsigned char *title = NULL;
	const unsigned char *section = NULL;
	const unsigned char *date = NULL;
	const unsigned char *source = NULL;
	const unsigned char *volume = NULL;

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

	PD = 1;
	IP = PP_INDENT;
	RM = 78;
	LM = PP_INDENT;

	th_source = NULL;
	header[0] = 0;
	footer[0] = 0;


	rs_count = 0;
	read_init(fp, filename);


	for(;;) {
		int x;
		const unsigned char *cp = read_text();
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
				set_font(FONT_R);
				set_in_ti(LM, -1);
				br_PD();
				break;

			case tkIP:
				/* indented paragraph */
				/* IP [tag [width]] */
				trap = 0;
				flush(0);
				set_font(FONT_R);
				br_PD();
				if (argc >= 2) {
					/* set IP width... */
					IP = get_unit(argv[1], PP_INDENT, -1);
				}

				if (argc >= 1) {
					set_in_ti(LM + IP, LM);
					set_tag(argv[0], IP);
				} else {
					set_in_ti(LM + IP, -1);
				}
				break;

			case tkHP:
				/* hanging paragraph */
				/* .HP [indent] */
				trap = 0;
				flush(0);
				set_font(FONT_R);
				if (argc >= 1) {
					/* set IP width... */
					IP = get_unit(argv[0], PP_INDENT, -1);
				}
				set_in_ti(LM + IP, LM);
				br_PD();
				break;

			case tkTQ:
			case tkTP:
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
				set_font(FONT_R);
				if (argc >= 1) {
					/* set IP width... */
					IP = get_unit(argv[0], PP_INDENT, -1);
				}
				set_in_ti(LM + IP, LM);
				if (type == tkTP) { br_PD(); }
				trap = tkTP;
				break;

			case tkSY:
				/*
					GNU extension
					.SY xxx -> .HP width(xxx) \ \fBxxx\fR
				*/
				/* warned in read2.c */
				trap = 0;
				flush(0);
				if (argc) {
					IP = xstrlen(argv[0]) + 1;					
				}
				set_in_ti(LM + IP, LM);
				br_PD();
				set_tag(argv[0], IP);
				break;
			case tkYS:
				flush(0);
				break;

			case tkEX:
				trap = 0;
				flush(0);
				fi = 0;
				break;
			case tkEE:
				trap = 0;
				fi = 1;
				break;


			case tkPD:
				PD = get_unit(argv[0], 1, -1);
				break;
			case tkRS:
				/* RS [width] */
				flush(0);
				rs_stack[rs_count++] = LM;
				if (argc >= 1) IP = get_unit(argv[0], PP_INDENT, -1);
				LM += IP;
				set_in(LM);
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
						/*warnx("Invalid RS count (%d / %d)", n + 1, rs_count);*/
						man_warnx1s("Invalid RS count %s", argv[0]);
					}

				} else {
					if (rs_count)
						LM = rs_stack[--rs_count];
					else LM = 0;
				}

				set_in(LM);
				break;

			case tkSH:
			case tkSS:
				/* next-line scoped */
				trap = 0;
				flush(0);
				set_font(FONT_R);

				na = 0;
				ad = 'b';
				hy = 1;
				fi = 1;

				LM = PP_INDENT;
				IP = PP_INDENT;
				if (type == tkSH && argc == 1 && !strcmp(argv[0], "SYNOPSIS")) {
					na = 1;
					hy = 0;
				}
				set_in_ti(PP_INDENT, type == tkSS ? SS_INDENT : SH_INDENT);
				br_PD();
				if (argc) {
					int i;
					for (i = 0; i < argc; ++i) append(argv[i]);
					flush(0);
				} else {
					trap = type;
				}
				break;

			case tkTE:
				/* end tbl */
				man_warnx("TE without TS");
				break;
			case tkTS:
				/* start tbl */
				flush(0);
				tbl();
				break;


			case tkTEXT:
				if (!fi) {
					print(cp, 0);
					fputc('\n', stdout); ++line;
					break;
				}
				if (trap == tkTP) {
					trap = 0;
					set_tag(cp, IP);
					cp = NULL;
				}
				if (!cp || !cp[0]) continue;
				append(cp);
				switch(trap) {
					case tkSS:
					case tkSH:
						flush(0);
						/* set_font(FONT_R); */
						/* set_in_ti(PP_INDENT, -1); */
						break;
				}
				trap = 0;
				break;
		}
	}
	flush(0);
	set_font(FONT_R);
	print_footer();
	read_init(NULL, NULL);
}

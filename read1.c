
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>

#include "man.h"


char *token_names[] = {
#define _(x) #x ,
#include "xtokens.h"
#undef _
""
};

#define MAX_ARGC 9

/*
 first half of the buffer is used for reading.
 line is too long if it extends into the second half.
 second half is used for expanding strings/characters, if necessary.
*/

static unsigned char buffer[MAN_BUFFER_SIZE * 2];
#define out_buffer buffer


int type = tkTEXT;
int argc = 0;
const unsigned char *argv[MAX_ARGC+1];
static uint_fast8_t cc = '.';
static uint_fast8_t c2 = '\'';
static uint_fast8_t ec = '\\';


/* registers */
unsigned ad = 'b'; /* .ad register */
unsigned fi = 1; /* .fi/.nf register */
unsigned na = 0;
unsigned hy = 1;
unsigned ns = 0; /* .ns / .rs */
unsigned ft = FONT_R;
unsigned prev_ft = FONT_R;
int ti = -1;
unsigned in = 0;
unsigned prev_in = 0;
unsigned ll = 78;
unsigned prev_ll = 78;

unsigned width = 78;
unsigned line = 1;

unsigned ignore = 0;

struct flags flags = { 0, 0 };

#define MAX_SO 4
struct so_entry {
	FILE *fp;
	char *name;
	unsigned line;
} so_entries[MAX_SO];
static int so_index = -1;

static void parse_text(int);
static void parse_args(unsigned, int);
static unsigned analyze(unsigned *);
extern const char *special_char(const unsigned char *);
extern const char *special_string(const unsigned char *);

static unsigned char arg_buffer[32];
/* read an argument return -1 on error */
/* 0 is length of empty [] argument... */
int get_arg(unsigned *i_ptr, unsigned type) {
	unsigned i = *i_ptr;
	uint_fast8_t c = type;

	if (type == 0) c = type = buffer[i++];
	arg_buffer[0] = 0;
	if (type == 0) return 0; /* error. */
	if (type == '[') {
		unsigned j = 0;
		for (;;) {
			c = buffer[i++];
			if (c == 0) return 0;
			if (c == ']') break;
			if (j < 31) arg_buffer[j] = c;
			++j;
		}
		if (j < 32)
			arg_buffer[j] = 0;
		else arg_buffer[31] = 0;
		*i_ptr = i;
		return j;
	}
	if (type == '(') {
		c = buffer[i++];
		if (!c) return 0;
		arg_buffer[0] = c;
		c = buffer[i++];
		if (!c) return 0;
		arg_buffer[1] = c;
		arg_buffer[2] = 0;
		*i_ptr = i;
		return 2;
	}
	arg_buffer[0] = c;
	arg_buffer[1] = 0;
	*i_ptr = i;
	return 1;
}




extern void render_init(void);
void read_init(FILE *fp, const char *name) {
	int i;

	type = fp ? tkTEXT : tkEOF;
	argc = 0;
	argv[0] = NULL;

	cc = '.';
	c2 = '\'';
	ec = '\\';

	ad = 'b'; 
	fi = 1;
	na = 0;
	hy = 1;
	ns = 0;

	ti = -1;
	in = prev_in = 0;
	ll = prev_ll = 78;
	ft = prev_ft = FONT_R;
	width = 78;
	line = 1;
	ignore = 0;

	render_init();


	for (i = 0; i <= so_index; ++i) free(so_entries[i].name);
	for (i = 1; i <= so_index; ++i) fclose(so_entries[i].fp);
	so_index = -1;

	if (fp) {
		so_index = 0;
		so_entries[0].fp = fp;
		so_entries[0].name = strdup(name);
		so_entries[0].line = 0;
	}
}


void man_errc(int eval, int e, const char *msg) {
	man_warnc(e, msg);
	exit(eval);
}

void man_errc1s(int eval, int e, const char *msg, const char *s) {
	man_warnc1s(e, msg, s);
	exit(eval);
}

void man_warnc(int e, const char *msg) {
	if (so_index >= 0) {
		fprintf(stderr, "%s:%d: ",
			so_entries[so_index].name,
			so_entries[so_index].line);
	}
	fputs(msg, stderr);
	if (e) {
		fputs(": ", stderr);
		fputs(strerror(e), stderr);
	}
	fputc('\n', stderr);
}

void man_warnc1s(int e, const char *msg, const char *s) {
	if (so_index >= 0) {
		fprintf(stderr, "%s:%d: ",
			so_entries[so_index].name,
			so_entries[so_index].line);
	}
	fprintf(stderr, msg, s);
	if (e) {
		fputs(": ", stderr);
		fputs(strerror(e), stderr);
	}
	fputc('\n', stderr);
}


extern int get_unit(const unsigned char *arg, int dv, int base);
extern void set_ll(int);
extern void set_ti(int);
extern void set_in(int);
extern void flush(unsigned justify);
extern void append_font(unsigned);

const unsigned char *read_line(void) {


	if (so_index < 0) { type = tkEOF; return NULL; }

	for(;;) {
		unsigned escape = 0;
		unsigned offset = 0;

		type = tkTEXT;
		argc = 0;
		argv[0] = NULL;


		for(;;) {
			int bits;
			char *cp;

			cp = fgets(buffer + offset, sizeof(buffer) - offset, 
				so_entries[so_index].fp);
			if (cp == NULL) {
				if (offset) break;


				if (ignore && flags.W >= 1) {
					man_warnx("end of input while ignoring lines");
				}

				free(so_entries[so_index].name);
				if (so_index) fclose(so_entries[so_index].fp);
				--so_index;
				if (so_index >= 0) continue;
				type = tkEOF;
				return NULL;
			}
			so_entries[so_index].line++;

			if (ignore) {
				if (buffer[0] == cc && buffer[1] == '.')
					ignore = 0;
				continue;
			}

			/* potentially false positive but no line should be this long */
			bits = analyze(&offset);
			if (offset > MAN_BUFFER_SIZE)
				man_errx(1, "line too long.");

			if (bits & 0x01) escape = 1;
			if (bits & 0x02) continue; /*  /n - join next line. */
			break;
		}
		if (buffer[0] == cc || buffer[0] == c2) {
			unsigned i = 1;
			uint_fast8_t c;
			char *cp;

			while (isspace(buffer[i])) ++i;
			c = buffer[i++];
			if (c == 0) continue;

#define _1(x1,y1,v1)\
case x1: c = buffer[i++];\
if (c == y1) type = v1;\
break;

#define _2(x1,y1,v1,y2,v2)\
case x1: c = buffer[i++];\
     if (c == y1) type = v1;\
else if (c == y2) type = v2;\
break;

#define _2x(x1,y1,v1,y2,v2,zz)\
case x1: c = buffer[i++];\
     if (c == y1) type = v1;\
else if (c == y2) type = v2;\
else { --i; type = zz; }\
break;

#define _3(x1,y1,v1,y2,v2,y3,v3)\
case x1: c = buffer[i++];\
     if (c == y1) type = v1;\
else if (c == y2) type = v2;\
else if (c == y3) type = v3;\
break;

#define _3x(x1,y1,v1,y2,v2,y3,v3,zz)\
case x1: c = buffer[i++];\
     if (c == y1) type = v1;\
else if (c == y2) type = v2;\
else if (c == y3) type = v3;\
else { --i; type = zz; }\
break;


#define _4(x1,y1,v1,y2,v2,y3,v3,y4,v4)\
case x1: c = buffer[i++];\
     if (c == y1) type = v1;\
else if (c == y2) type = v2;\
else if (c == y3) type = v3;\
else if (c == y4) type = v4;\
break;

#define _4x(x1,y1,v1,y2,v2,y3,v3,y4,v4,zz)\
case x1: c = buffer[i++];\
     if (c == y1) type = v1;\
else if (c == y2) type = v2;\
else if (c == y3) type = v3;\
else if (c == y4) type = v4;\
else { --i; type = zz; }\
break;

#define _5(x1,y1,v1,y2,v2,y3,v3,y4,v4,y5,v5)\
case x1: c = buffer[i++];\
     if (c == y1) type = v1;\
else if (c == y2) type = v2;\
else if (c == y3) type = v3;\
else if (c == y4) type = v4;\
else if (c == y5) type = v5;\
break;


/*
 * Not supported:
 * .TS / .TE -> tbl
 * .EQ / .EN -> eqn
 * .PS / .PE -> pic
 * .G1 / .G2 -> grap
 * .[ / .] / .R1 / .R2 -> refer
 * .vS / .vE -> vgrind
 */

			switch(c) {
			/* tkxx skipped tokens:
				.ad [blrc] - text justification
				.if ...
				.nh - no hyphenation
				.ne # - page break unless >= N lines available.
				.cc - command char (.)
				.ec - escape char (\)
				.eo - escape char off.
			*/
			_2 ('a', 'b', tkab, 'd', tkad);
			_1 ('b', 'r', tkbr);
			_2 ('c', 'c', tkcc, '2', tkc2);
			_2 ('e', 'c', tkec, 'o', tkeo);
			_2 ('f', 'i', tkfi, 't', tkft);
			_1 ('h', 'y', tkhy);
			_3 ('i', 'n', tkin, 'f', tkxx, 'g', tkig);
			_1 ('l', 'l', tkll);
			_5 ('n', 'f', tknf, 'a', tkna, 'e', tkxx, 'h', tknh, 's', tkns);
			_1 ('r', 's', tkrs);
			_2 ('s', 'o', tkso, 'p', tksp);
			_2 ('t', 'i', tkti, 'm', tktm);
			/* */
			_1 ('A', 'T', tkAT);
			_2x('B', 'I', tkBI, 'R', tkBR, tkB);
			_1 ('D', 'T', tkDT);
			_2 ('E', 'E', tkEE, 'X', tkEX);
			_1 ('H', 'P', tkHP);
			_4x('I', 'B', tkIB, 'P', tkIP, 'R', tkIR, 'X', tkIX, tkI);
			_1 ('L', 'P', tkLP);
			_3 ('M', 'E', tkME, 'R', tkMR, 'T', tkMT);
			_1 ('O', 'P', tkOP);
			_2x('P', 'D', tkPD, 'P', tkPP, tkP);
			_4x('R', 'B', tkRB, 'E', tkRE, 'I', tkRI, 'S', tkRS, tkR);
			_5 ('S', 'B', tkSB, 'H', tkSH, 'M', tkSM, 'S', tkSS, 'Y', tkSY);
			_3 ('T', 'H', tkTH, 'P', tkTP, 'Q', tkTQ);
			_3 ('U', 'C', tkUC, 'E', tkUE, 'R', tkUR);
			_1 ('Y', 'S', tkYS);
			}
			if (type == tkTEXT) {
				if (flags.W) man_warnx1s("invalid command: %s", buffer);
				continue;
			}
			c = buffer[i];
			if (c != 0 && !isspace(c)) {
				if (flags.W) man_warnx1s("invalid command: %s", buffer);
				continue;
			}
			while (isspace((c = buffer[i]))) ++i;
			cp = buffer + i;


			if (flags.W >= 3) switch(type) {
				case tkIX:
					man_warnx1s("unsupported macro: .%s", token_names[type]);
					break;
				case tkUR: case tkUE: case tkMT: case tkME:
				case tkOP: case tkSY: case tkYS: case tkTQ:
					man_warnx1s("non-standard GNU macro: .%s", token_names[type]);
				case tkMR:
					man_warnx1s("non-standard Plan 9 macro: .%s", token_names[type]);
					break;
			}
			switch(type) {
				case tkUR: case tkUE: case tkMT: case tkME: case tkIX:
					continue;
				case tkxx: continue;
				case tkcc:
					/* quotes not supported. */
					cc = c == 0 ? '.' : c;
					continue;
				case tkc2:
					/* quotes not supported. */
					c2 = c == 0 ? '\'' : c;
					continue;
				case tkeo: ec = 0; continue;
				case tkec:
					ec = c == 0 ? '\\' : c;
					continue;

				case tkab:
					/* print message to stderr and abort */
					fputs(c ? cp : "User abort.", stderr);
					fputs("\n", stderr);
					exit(1);
					continue;

				case tktm:
					/* print message to stderr */
					if (c) fprintf(stderr, "%s\n", cp);
					continue;


				case tkad:
					na = 0;
					if (c == 'b' || c == 'c' || c == 'l' || c == 'r')
						ad = c;
					continue;
				case tkna: na = 1; continue;
				case tknh: hy = 0; continue;
				case tkhy: {
					/* .hy # */
					/* 0 = off, 1 = on, 2,4,8 have other implications */
					if (c == '0') hy = 0;
					else hy = 1;
					continue;
				}

				case tkrs: ns = 0; continue;
				case tkns: ns = 1; continue;

				case tknf: flush(0); fi = 0; continue;
				case tkfi: flush(0); fi = 1; continue;

				case tkbr:
					flush(0);
					continue;
				case tksp: {
					int n;
					flush(0);
					if (ns) break;
					n = get_unit(cp, 1, -1);
					while (--n >= 0 ) {
						fputc('\n', stdout); ++line;
					}
					continue;
				}



				case tkso: {
					/* heirloom troff so supports quotes. */
					/* mandoc and groff do not. */
					/* groff strips trailing ws; mandoc does not */
					if (c) {
						FILE *fp;
						if (so_index == MAX_SO-1) {
							man_warnx("too many .so requests.");
							continue;
						}
						fp = fopen(cp, "r");
						if (!fp) {
							man_warn1s(".so %s", cp);
							continue;
						}
						++so_index;
						so_entries[so_index].fp = fp;
						so_entries[so_index].name = strdup(cp);
					}
					continue;
				}

				case tkll: {
					/* .ll +N */
					/* default is previous .ll */
					int n;
					flush(0);
					n = get_unit(cp, prev_ll, ll);
					set_ll(n);
					continue;
				}

				case tkin: {
					/* .in +N */
					/* default is previous .in */
					int n;
					flush(0);
					n = get_unit(cp, prev_in, in);
					set_in(n);
					continue;
				}

				case tkti: {
					/* .ti +N */
					/* relative to in */
					int n;
					flush(0);
					n = get_unit(cp, -1, in);
					set_ti(n);
					continue;
				}

				case tkft: {
					/* .ft [BRIP123] */
					/* default = previous */
					unsigned ff;
					switch(c) {
						default:
						case 'P': ff = FONT_P; break;
						case 'R': case '1': ff = FONT_R; break;
						case 'B': case '3': ff = FONT_B; break;
						case 'I': case '2': ff = FONT_I; break;
					}
					append_font(ff);
					continue;
				}

				case tkig:
					/* ig [end] */
					/* todo -- support end argument */
					ignore = 1;
					continue;

				default:
					parse_args(i, sizeof(buffer) - offset);
					return "";
				}
		}
		type = tkTEXT;
		if (!escape) return buffer;

		parse_text(MAN_BUFFER_SIZE - offset);
		return buffer + MAN_BUFFER_SIZE;
	}
}



/*
 * 5 things...
 * 1. strip comments
 * 2. kill incomplete escape sequences
 * 3. check for escape sequences
 * 4. check for a continuation sequence.
 * 5. strip unsupported escape sequences.
 * 
 * N.B. - characters/strings aren't expanded, or
 * anything that would interfere with macro argument parsing.
 */
static unsigned analyze(unsigned *i_ptr) {
	/*
		& 0x01 -> escape sequence detected
		& 0x02 -> continuation detected.
		& 0x04 -> invalid escape sequence detected (and removed)
		& 0x08 -> unsupported escape sequence removed. (or font replacement)
	*/
	unsigned i = *i_ptr;
	unsigned j;
	unsigned kill = 0;
	uint_fast8_t c;
	unsigned rv = 0;

	for (; ;) {
		j = i;
		c = buffer[i++];
		if (c == 0) return rv;
		if (c == '\r' || c == '\n')  goto exit;
		if (c == ' ' || c == '\t') continue;
		if (c >= 0x80 || c < 0x20) { buffer[j] = XSPACE; rv |= 0x08; continue; }
		if (c != ec) continue;
		c = buffer[i++];
		switch(c) {
			default:
				rv |= 0x01;
				break;

			case 0:
			case '\r':
			case '\n':
				rv |= 0x02;
				goto exit;
				break;

			case '"': /* comment */
				goto exit;
			case '#': /* groff comment */
				if (flags.W >= 3) {
					man_warnx("non-standard GNU # comment");					
				}
				rv |= 0x02;
				goto exit;

			case '*':
				/* \*X, \*(XX, or \*[...] */
				c = buffer[i++];
				if (c == 0) goto invalid;
				if (c == '(') goto paren;
				if (c == '[') goto bracket;
				rv |= 0x01;
				break;

			case '[':
			bracket:
				for(;;) {
					c = buffer[i++];
					if (c == 0) goto invalid;
					if (c == ']') break;
				}
				rv |= 0x01;
				break;

			case '(':
			paren:
				c = buffer[i++];
				if (c == 0) goto invalid;
				c = buffer[i++];
				if (c == 0) goto invalid;
				rv |= 0x01;
				break;

#if 0
			case 'f':
				c = buffer[i++];
				if (c == 'C') c = buffer[i++];
				if (c == 0) goto invalid;
				if (c == '[') goto bracket;
				if (c == '(') goto paren;
				rv |= 0x01;
				break;
#endif

			case 'f':
				/* \fC?[BIRP] */
				/* \fC?[321] */
				/* \f(BI */
				c = buffer[i++];
				if (c == 'C') c = buffer[i++];
				if (get_arg(&i, c) < 0) goto invalid;
				kill = 1;
				switch(arg_buffer[0]) {
					default:
					case 'R': case '1': buffer[j++] = FONT_R; break;
					case 'B': case '3': buffer[j++] = FONT_B; break;
					case 'I': case '2': buffer[j++] = FONT_I; break;
					case 'P': buffer[j++] = FONT_P; break;
				}
				break;

			/* everything below are stripped. */
			case 'F': case 'g': case 'k': case 'M': case 'm': 
			case 'n': case 'V': case 'Y':
				c = buffer[i++];
				if (c == 0) goto invalid;
				kill = 1;
				if (c == '(') goto paren;
				if (c == '[') goto bracket;

				break;

			case 'A': case 'B': case 'b': case 'C': case 'D': 
			case 'H': case 'h': case 'L': case 'l': case 'N': 
			case 'o': case 'R': case 'S': case 's': case 'v': 
			case 'w': case 'X': case 'x': case 'Z':
				c = buffer[i++];
				if (c != '\'') goto invalid;
				for(;;) {
					c = buffer[i++];
					if (c == 0) goto invalid;
					if (c == '\'') break;
				}
				rv |= 0x01;
				kill = 1;
				break;
		}
		if (kill) {
			rv |= 0x08;
			while (j < i) buffer[j++] = XSPACE;
			kill = 0;
		}
	}

invalid:
	rv |= 0x04;

exit:
	buffer[j] = 0;

	if (rv & 0x08) {
		/* remove XSPACEs. */
		i = j = *i_ptr;

		for(;;++i) {
			c = buffer[i];
			if (c == 0) break;
			if (c == XSPACE) continue;
			buffer[j++] = c;
		}
		buffer[j] = 0;
	}
	*i_ptr = j;

	return rv;
}


void parse_args(unsigned i, int available) {
	unsigned j = MAN_BUFFER_SIZE;
	argc = 0;


	for(;;) {
		uint_fast8_t c;
		unsigned quote = 0;
		while (isspace(buffer[i])) ++i;

		c = buffer[i];
		if (c == 0) goto _break;
		if (c == '"') { quote = 1; ++i; }
		if (argc == MAX_ARGC) {
			argv[argc] = NULL;
			return;
		}
		argv[argc++] = out_buffer + j;

		for(;;) {

			c = buffer[i++];
			if (c == 0) goto _break;
			if (c == XSPACE) continue;
			if (quote && c == '"') {
				if (buffer[i] == '"') {
					out_buffer[j++] = '"';
					++i;
					continue;
				}
				break;
			}
			if (!quote && isspace(c)) {
				break;
			}

			if (c != ec) {
				out_buffer[j++] = c;
				continue;
			}


			c = buffer[i++];
			switch(c) {
				/* TODO - escape.h should check for buffer overflow */
				#include "escape.h"
			}
	
		}
		out_buffer[j++] = 0;
	}

_break:
	out_buffer[j++] = 0;
	argv[argc] = NULL;
}

static void parse_text(int available) {
	unsigned i;
	unsigned j;
	for (i = 0, j = MAN_BUFFER_SIZE;;) {
		uint_fast8_t c = buffer[i++];
		if (c == XSPACE) continue;
		if (c != ec) {
			out_buffer[j++] = c;
			if (c == 0) return;
			continue;
		}

		c = buffer[i++];
		switch(c) {
			/* TODO - escape.h should check for buffer overflow */
			#include "escape.h"
		}
	}
_break:
	out_buffer[j++] = 0;
}

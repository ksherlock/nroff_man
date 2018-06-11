
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include "man.h"


#define MAX_ARGC 9

static char buffer[4096];
static char out_buffer[4096];
int type = tkTEXT;
int argc = 0;
const char *argv[MAX_ARGC+1];
static char cc = '.';
static char ec = '\\';


/* registers */
unsigned ad = 'b'; /* .ad register */
unsigned fi = 1; /* .fi/.nf register */
unsigned na = 0;
unsigned hy = 1;


struct flags flags = { 0, 0 };

#define MAX_SO 4
struct so_entry {
	FILE *fp;
	char *name;
	unsigned line;
} so_entries[MAX_SO];
static int so_index = -1;

static void parse_text(void);
static void parse_args(unsigned);
static unsigned analyze(unsigned *);
extern const char *special_char(const char *);
extern const char *special_string(const char *);

static char arg_buffer[32];
/* read an argument return -1 on error */
/* 0 is length of empty [] argument... */
int get_arg(unsigned *i_ptr, unsigned type) {
	unsigned i = *i_ptr;
	unsigned char c = type;

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




void read_init(FILE *fp, const char *name) {
	int i;

	type = fp ? tkTEXT : tkEOF;
	buffer[0] = 0;
	out_buffer[0] = 0;
	argc = 0;
	argv[0] = NULL;
	cc = '.';
	ec = '\\';


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


const char *read_line(void) {


	if (so_index < 0) { type = tkEOF; return NULL; }

	for(;;) {
		unsigned escape = 0;
		unsigned offset = 0;

		type = tkTEXT;
		argc = 0;
		argv[0] = NULL;


		for(;;) {
			int bits;
			char *cp = fgets(buffer + offset, sizeof(buffer) - offset, so_entries[so_index].fp);
			if (cp == NULL) {
				if (offset) break;

				free(so_entries[so_index].name);
				if (so_index) fclose(so_entries[so_index].fp);
				--so_index;
				if (so_index >= 0) continue;
				type = tkEOF;
				return NULL;
			}
			so_entries[so_index].line++;

			bits = analyze(&offset);
			if (bits & 0x01) escape = 1;
			if (bits & 0x02) continue; /*  /n - join next line. */
			break;
		}
		if (buffer[0] == cc) {
			unsigned i = 1;
			unsigned char c;
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
			_1 ('a', 'd', tkad);
			_1 ('b', 'r', tkbr);
			_1 ('c', 'c', tkcc);
			_2 ('e', 'c', tkec, 'o', tkeo);
			_1 ('f', 'i', tkfi);
			_1 ('h', 'y', tkhy);
			_2 ('i', 'n', tkin, 'f', tkxx);
			_4 ('n', 'f', tknf, 'a', tkna, 'e', tkxx, 'h', tknh);
			_2 ('s', 'o', tkso, 'p', tksp);
			/* */
			_1 ('A', 'T', tkAT);
			_2x('B', 'I', tkBI, 'R', tkBR, tkB);
			_1 ('D', 'T', tkDT);
			_2 ('E', 'E', tkEE, 'X', tkEX);
			_1 ('H', 'P', tkHP);
			_3x('I', 'B', tkIB, 'P', tkIP, 'R', tkIR, tkI);
			_1 ('L', 'P', tkLP);
			_2 ('M', 'E', tkME, 'T', tkMT);
			_1 ('O', 'P', tkOP);
			_2x('P', 'D', tkPD, 'P', tkPP, tkP);
			_4x('R', 'B', tkRB, 'E', tkRE, 'I', tkRI, 'S', tkRS, tkR);
			_5 ('S', 'B', tkSB, 'H', tkSH, 'M', tkSM, 'S', tkSS, 'Y', tkSY);
			_3 ('T', 'H', tkTH, 'P', tkTP, 'Q', tkTQ);
			_3 ('U', 'C', tkUC, 'E', tkUE, 'R', tkUR);
			_1 ('Y', 'S', tkYS);
			}
			if (type == tkTEXT) {
				warnx("invalid command: %s", buffer);
				continue;
			}
			c = buffer[i++];
			if (c != 0 && !isspace(c)) {
				warnx("invalid command: %s", buffer);
				continue;
			}
			switch(type) {
				case tkxx: continue;
				case tkcc:
					/* quotes not supported. */
					while (isspace(c = buffer[i])) ++i;
					cc = c == 0 ? '.' : c;
					continue;
				case tkeo: ec = 0; continue;
				case tkec:
					while (isspace(c = buffer[i])) ++i;
					ec = c == 0 ? '\\' : c;
					continue;

				case tkad:
					na = 0;
					while (isspace(c = buffer[i])) ++i;
					if (c == 'b' || c == 'c' || c == 'l' || c == 'r')
						ad = c;
					continue;
				case tkna: na = 1; continue;
				case tknh: hy = 0; continue;
				case tkhy: {
					/* .hy # */
					/* 0 = off, 1 = on, 2,4,8 have other implications */
					while (isspace(c = buffer[i])) ++i;
					if (c == '0') hy = 0;
					else hy = 1;
					continue;
				}

				case tkso: {
					/* heirloom troff so supports quotes. */
					/* mandoc and groff do not. */
					/* groff strips trailing ws; mandoc does not */
					char *cp;
					while (isspace(buffer[i])) ++i;
					cp = buffer + i;
					if (cp[0]) {
						FILE *fp;
						if (so_index == MAX_SO-1) {
							warnx("too many .so requests.");
							continue;
						}
						fp = fopen(cp, "r");
						if (!fp) {
							warn(".so %s", cp);
							continue;
						}
						++so_index;
						so_entries[so_index].fp = fp;
						so_entries[so_index].name = strdup(cp);
					}
					continue;
				}

				default:
					parse_args(i);
					return "";
				}
		}
		type = tkTEXT;
		if (!escape) return buffer;

		parse_text();
		return out_buffer;
	}
}





/*
 * 3 things...
 * 1. strip comments
 * 2. kill incomplete escape sequences
 * 3. check for escape sequences
 * 4. check for a continuation sequence.
 * replace unsupported escapes w/ 0-width space?
 */
static unsigned analyze(unsigned *i_ptr) {
	/*
		& 0x01 -> escape sequence detected
		& 0x02 -> continuation detected.
		& 0x04 -> invalid escape sequence detected (and removed)
	*/
	unsigned i = *i_ptr;
	unsigned j;
	unsigned escape = 0;
	unsigned kill = 0;
	unsigned char c;

	for (; ;) {
		j = i;
		c = buffer[i++];
		if (c == 0) return escape;
		if (c == '\r' || c == '\n') {
			buffer[j] = 0;
			*i_ptr = j;
			return escape;
		}
		if (c == ' ' || c == '\t') continue;
		if (c >= 0x80 || c < 0x20) { buffer[j] = ZWSPACE; escape = 1; continue; }
		if (c != ec) continue;
		c = buffer[i++];
		switch(c) {
			default:
				escape = 1;
				break;

			case 0:
			case '\r':
			case '\n':
				buffer[j] = 0;
				*i_ptr = j;
				return 0x02 | escape;
				break;

			case '"': /* comment */
				buffer[j] = 0;
				*i_ptr = j;
				return escape;

			case '*':
				/* \*X, \*(XX, or \*[...] */
				j = i-2;
				c = buffer[i++];
				if (c == 0) goto invalid;
				if (c == '(') goto paren;
				if (c == '[') goto bracket;
				escape = 1;
				break;

			case '[':
			bracket:
				for(;;) {
					c = buffer[i++];
					if (c == 0) goto invalid;
					if (c == ']') break;
				}
				escape = 1;
				break;

			case '(':
			paren:
				j = i-2;
				c = buffer[i++];
				if (c == 0) goto invalid;
				c = buffer[i++];
				if (c == 0) goto invalid;
				escape = 1;
				break;

			case 'f':
				c = buffer[i++];
				if (c == 'C') c = buffer[i++];
				if (c == 0) goto invalid;
				if (c == '[') goto bracket;
				if (c == '(') goto paren;
				escape = 1;
				break;

			case 'F': case 'g': case 'k': case 'M': case 'm': 
			case 'n': case 'V': case 'Y':
				c = buffer[i++];
				if (c == 0) goto invalid;
				if (c == '(') goto paren;
				if (c == '[') goto bracket;
				escape = 1;
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
				escape = 1;
				break;
		}
		if (kill) {
			escape = 1;
			while (j < i) buffer[j++] = ZWSPACE;
		}
	}

invalid:
	buffer[j] = 0;
	*i_ptr = j;
	return 0x04 | escape;
}

void parse_args(unsigned i) {
	unsigned j = 0;
	argc = 0;


	for(;;) {
		unsigned char c;
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
			if (c == ZWSPACE) continue;
			/* don't strip -- needed for .\~ etc, */
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
				#include "escape.h"
			}
	
		}
		out_buffer[j++] = 0;
	}

_break:
	out_buffer[j++] = 0;
	argv[argc] = NULL;
}

static void parse_text(void) {
	unsigned i;
	unsigned j;
	for (i = 0, j = 0;;) {
		unsigned char c = buffer[i++];
		if (c == ZWSPACE) continue;
		if (c != ec) {
			out_buffer[j++] = c;
			if (c == 0) return;
			continue;
		}

		c = buffer[i++];
		switch(c) {
			#include "escape.h"
		}
	}
_break:
	out_buffer[j++] = 0;
}

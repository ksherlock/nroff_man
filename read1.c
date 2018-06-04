
#include <ctype.h>
#include <stdio.h>
#include <err.h>
#include "man.h"


static char buffer[4096];
static char out_buffer[4096];
int type = tkTEXT;
int argc = 0;
const char *argv[32];
static int eof = 0;

static FILE *infile = NULL;

static void parse_text(void);
static void parse_args(unsigned);
static unsigned analyze(unsigned *);
extern const char *special_char(const char *);

static char arg_buffer[32];
/* read an argument return 0 on error */
unsigned get_arg(unsigned *i_ptr, unsigned type) {
	unsigned i = *i_ptr;
	unsigned char c = type;

	if (type == 0) c = type = buffer[i++];
	arg_buffer[0] = 0;
	if (type == 0) return 0; // error.
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
	buffer[0] = c;
	buffer[1] = 0;
	*i_ptr = i;
	return 1;
}




void read_init(FILE *fp) {
	infile = fp;
	type = fp ? tkTEXT : tkEOF;
	eof = fp ? 0 : 1;
	buffer[0] = 0;
	out_buffer[0] = 0;
	argc = 0;
	argv[0] = NULL;
}

const char *read_line(void) {


	if (eof) { type = tkEOF; return NULL; }

	for(;;) {
		type = tkTEXT;
		unsigned escape = 0;
		unsigned offset = 0;
		argc = 0;
		argv[0] = NULL;


		for(;;) {
			int bits;
			char *cp = fgets(buffer + offset, sizeof(buffer) - offset, infile);
			if (cp == NULL) {
				if (offset) break;
				type = tkEOF;
				return NULL;
			}

			bits = analyze(&offset);
			if (bits & 0x01) escape = 1;
			if (bits & 0x02) continue; /*  /n - join next line. */
			break;
		}
		if (buffer[0] == '.') {
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


			switch(c) {
			_1 ('f', 'i', tkFI);
			_1 ('i', 'n', tkIN);
			_1 ('n', 'f', tkNF);
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
			_4 ('S', 'B', tkSB, 'H', tkSH, 'M', tkSM, 'S', tkSS);
			_2 ('T', 'H', tkTH, 'P', tkTP);
			_3 ('U', 'C', tkUC, 'E', tkUE, 'R', tkUR);
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
			parse_args(i);
			return "";
		}
		type = tkTEXT;
		if (!escape) return buffer;

		parse_text();
		return out_buffer;
	}
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
		argv[argc++] = out_buffer + j;

		for(;;) {

			c = buffer[i++];
			if (c == 0) goto _break;
			if (c == ZWSPACE) continue;
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

			if (c == '\\') {
				//...
				c = buffer[i++];
				switch(c) {
					case 0: goto _break;

					case '&': case '|': case '^': case '%':
						//out_buffer[j++] = ZWSPACE; break;
						break;
						/* \% is a hyphenation point. */
					case ' ': case '0':
						out_buffer[j++] = NBSPACE; break; /* non-paddable */
					case 'e': out_buffer[j++] = '\\'; break;
					case '~': out_buffer[j++] = NBSPACE; break; /* paddable */
					case '"': goto _break; break; /* comment */
					case 't': out_buffer[j++] = '\t'; break;

					case '{': case '}': case 'd': case 'u': case 'p':
					case 'z':
						break;

					case '(': {
						unsigned k = 0;
						const char *cp;
						if (!get_arg(&i, '(')) goto _break;
						cp = special_char(arg_buffer);
						while ((c = cp[k++])) out_buffer[j++] = c;
						break;
					}
					case '*':
						/* pre-defined strings.. */
						/* \*X or \*(XX  \*[...] */
						if (!get_arg(&i, 0)) goto _break;
						break;

					case '[': {
						/* [name] - not yet supported. */
						for (;;) {
							c = buffer[i++];
							if (c == 0) goto _break;
							if (c == ']') break;
						}
						break;
					}

					case 'f': {
						/* \fC?[BIRP] */
						/* \fC?[321] */
						/* \f(BI */
						if (buffer[i] == 'C') ++i;
						if (!get_arg(&i, 0)) goto _break;
						// todo...
						break;
					}
					case 'F': case 'g': case 'k': case 'M': case 'm': 
					case 'n': case 'V': case 'Y':
						if (!get_arg(&i, 0)) goto _break;
						break;

					case 'A': case 'B': case 'b': case 'C': case 'D': 
					case 'H': case 'h': case 'L': case 'l': case 'N': 
					case 'o': case 'R': case 'S': case 's': case 'v': 
					case 'w': case 'X': case 'x': case 'Z':
						{
							/* A'string', etc */
							c = buffer[i++]; if (c == 0) goto _break;
							for(;;) {
								c = buffer[i++];
								if (c == 0) goto _break;
								if (c == '\'') break;
							}
							break;
						}
					default: out_buffer[j++] = c; break;
				}
				continue;
			}
			out_buffer[j++] = c;
		}
		out_buffer[j++] = 0;
	}

_break:
	out_buffer[j++] = 0;
	argv[argc] = NULL;
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
	// & 0x01 -> escape sequence detected
	// & 0x02 -> continuation detected.
	// & 0x04 -> invalid escape sequence detected (and removed)
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
		if (c != '\\') continue;
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

static void parse_text(void) {
	unsigned i;
	unsigned j;
	for (i = 0, j = 0;;) {
		unsigned char c = buffer[i++];
		if (c == ZWSPACE) continue;
		// todo...
		out_buffer[j++] = c;
		if (c == 0) break;

	}
}
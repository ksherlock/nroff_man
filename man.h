

extern int type;
extern int argc;
extern const char *argv[];

extern unsigned na;
extern unsigned ad; /* .j */
extern unsigned fi; /* .u */

extern struct flags {
	unsigned W;
	const char *os;
} flags;

enum {
	NBSPACE = 0x80,
	XSPACE,
	ZWSPACE,
	FONT_R,
	FONT_B,
	FONT_I,
	FONT_P,
	HYPHEN,
};


enum {
	tkEOF,
	tkTEXT,
	/**/
	tkAT,
	tkB,
	tkBI,
	tkBR,
	tkDT,
	tkEE,
	tkEX,
	tkHP,
	tkI,
	tkIB,
	tkIP,
	tkIR,
	tkLP,
	tkME,
	tkMT,
	tkOP,
	tkP,
	tkPD,
	tkPP,
	tkR,
	tkRB,
	tkRE,
	tkRI,
	tkRS,
	tkSB,
	tkSH,
	tkSM,
	tkSS,
	tkTH,
	tkTP,
	tkUC,
	tkUE,
	tkUR,
	tkSY,
	tkYS,
	tkTQ,
	/**/
	tkbr,
	tkfi,
	tkin,
	tknf,
	tkso,
	tksp,
	tkcc,
	tkec,
	tkeo,
	tkad,
	tkna,
	tkxx /* skipped */
};


const char *read_text(void);
const char *read_line(void);

void read_init(FILE *fp, const char *filename);
void man(FILE *fp, const char *filename);
void man_init(void);

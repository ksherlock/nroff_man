

extern int type;
extern int argc;
extern const char *argv[];

enum {
	NBSPACE = 0x80,
	ZWSPACE,
	FONT_R,
	FONT_B,
	FONT_I,
	FONT_P,
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
	/**/
	tkFI,
	tkIN,
	tkNF,
};


const char *read_text(void);
const char *read_line(void);

void read_init(FILE *fp);
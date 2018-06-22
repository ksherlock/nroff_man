

extern int type;
extern int argc;
extern const unsigned char *argv[];

extern unsigned na;
extern unsigned ad; /* .j */
extern unsigned fi; /* .u */
extern unsigned hy;

extern struct flags {
	unsigned W;
	unsigned i;
	unsigned b;
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
	tknh,
	tkhy,
	tkns,
	tkrs,
	tkxx /* skipped */
};


#define MAN_BUFFER_SIZE 2048

const unsigned char *read_text(void);
const unsigned char *read_line(void);

void read_init(FILE *fp, const char *filename);
void man(FILE *fp, const char *filename);
void man_init(void);


#define man_warn(msg) man_warnc(errno, msg)
#define man_warnx(msg) man_warnc(0, msg)

#define man_warn1s(msg,a) man_warnc1s(errno, msg, a)
#define man_warnx1s(msg,a) man_warnc1s(0, msg, a)

#define man_err(eval, msg) man_errc(eval, errno, msg)
#define man_errx(eval, msg) man_errc(eval, 0, msg)

#define man_err1s(eval, msg,a) man_errc1s(eval, errno, msg, a)
#define man_errx1s(eval, msg,a) man_errc1s(eval, 0, msg, a)

void man_warnc(int e, const char *msg);
void man_warnc1s(int e, const char *msg, const char *s);
void man_errc(int eval, int e, const char *msg);
void man_errc1s(int eval, int e, const char *msg, const char *s);

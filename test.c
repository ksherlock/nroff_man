#include <stdio.h>

#include "man.h"

const char *special_char(const char *cp) { return ""; }
int main(void) {

	man(stdin);
#if 0
	read_init(stdin);

	for(;;) {
		const char *cp = read_text();
		if (cp == NULL) break;
		if (type == tkTEXT) {
			fputs(cp, stdout);
			fputc('\n', stdout);
		} else {
			int i;
			for (i = 0; i < argc; ++i) {
				printf("  %s\n", argv[i]);
			}
		}
	}
	#endif
	return 1;
}

struct chars {
	unsigned short key;
	const char *value;
	unsigned short padding;
};

static struct chars char_table[] = {

#include "chars.h"

};

static struct chars string_table[] = {
#define PREDEF(a,b,c) { (a << 8) + b, c }, 
#include "strings.h"

};


/*
 * specialchar
 *	handles \(xx escape sequences for special characters
 */

const char *special_char(const char *s) {

	unsigned begin = 0;
	unsigned end = sizeof(char_table) / sizeof(char_table[0]);

	unsigned short target = (s[0] << 8) | (s[1] << 0);

	/* binary search */
	for (;;) {
		
		unsigned mid = (begin+end)>>1;
		unsigned key = char_table[mid].key;
		if (key == target) return char_table[mid].value;

		if (key < target) {
			begin = mid + 1;
			if (begin >= end) return "";
			continue;
		}
		/* key > target  */
		end = mid;
		if (end <= begin) return "";
		continue;
		
	}
}


const char *special_string(const char *s) {

	unsigned begin = 0;
	unsigned end = sizeof(string_table) / sizeof(string_table[0]);

	unsigned short target = (s[0] << 8) | (s[1] << 0);

	/* binary search */
	for (;;) {
		
		unsigned mid = (begin+end)>>1;
		unsigned key = string_table[mid].key;
		if (key == target) return string_table[mid].value;

		if (key < target) {
			begin = mid + 1;
			if (begin >= end) return "";
			continue;
		}
		/* key > target  */
		end = mid;
		if (end <= begin) return "";
		continue;
		
	}
}



#if defined(TEST)
#include <stdio.h>
#include <assert.h>
#include <string.h>
int main(int argc, char **argv) {

	int i;
	for (i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
		char lookup[3];
		lookup[0] = table[i].key >> 8;
		lookup[1] = table[i].key & 0xff;
		lookup[2] = 9;
		assert(table[i].value == specialchar(lookup));
	}
	assert(!strlen(specialchar("xx")));
	return 0;
}

#endif

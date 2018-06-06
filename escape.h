case 0: goto _break;

case '&': case '|': case '^': case '%':
	/* out_buffer[j++] = ZWSPACE; break; */
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
	switch(arg_buffer[0]) {
		case 'R': case '1': out_buffer[j++] = FONT_R; break;
		case 'B': case '3': out_buffer[j++] = FONT_B; break;
		case 'I': case '2': out_buffer[j++] = FONT_I; break;
		case 'P': out_buffer[j++] = FONT_P; break;
	}
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

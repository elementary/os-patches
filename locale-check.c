#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *help = "locale-check DEFAULT_LOCALE\n"
	"\n"
	"Check that the various locale-related environment variables contain\n"
	"values that can be set. Output shell that can be passed to eval to\n"
	"set any invalid environment variables to DEFAULT_LOCALE\n";

static void usage(void) {
	fprintf(stderr, "%s", help);
	exit(1);
}

static void check(int category, char* varname, char* defvalue) {
	if (getenv(varname) != NULL) {
		if (setlocale(category, "") == NULL) {
			printf("%s=%s\n", varname, defvalue);
		}
	}
}

#define SINGLEQUOTE '\''
#define BACKSLASH   '\\'

/* Quote 'val' for shell */
static char *quote(char* val) {
	/* This implementation single quotes val and replaces single quotes
	   with SINGLEQUOTE BACKSLASH SINGLEQUOTE SINGLEQUOTE. The worst
	   case is that val is entirely single quotes, in which case each
	   character of the input becomes 4 bytes. Then 3 bytes for
	   surrounding quotes and terminating NUL. */
	char *ret = malloc(strlen(val)*4+3);
	char *source = val;
	char *dest = ret;

	*dest++ = SINGLEQUOTE;
	while (*source) {
		if (*source == SINGLEQUOTE) {
			*dest++ = SINGLEQUOTE;
			*dest++ = BACKSLASH;
			*dest++ = SINGLEQUOTE;
		}
		*dest++ = *source++;
	}
	*dest++ = SINGLEQUOTE;
	*dest++ = 0;
	return ret;
}

#define CHECK(cat, def) check(cat, #cat, def);

int main(int argc, char** argv) {
	char *defval;
	if (argc != 2) {
		usage();
	}
	defval = quote(argv[1]);
	/* setlocale will never consult LANG if LC_ALL is set */
	if (getenv("LC_ALL") == NULL) {
		check(LC_ALL, "LANG", defval);
	} else {
		CHECK(LC_ALL, defval);
	}
	CHECK(LC_ADDRESS, defval);
	CHECK(LC_COLLATE, defval);
	CHECK(LC_CTYPE, defval);
	CHECK(LC_IDENTIFICATION, defval);
	CHECK(LC_MEASUREMENT, defval);
	CHECK(LC_MESSAGES, defval);
	CHECK(LC_MONETARY, defval);
	CHECK(LC_NAME, defval);
	CHECK(LC_NUMERIC, defval);
	CHECK(LC_PAPER, defval);
	CHECK(LC_TELEPHONE, defval);
	CHECK(LC_TIME, defval);
	return 0;
}

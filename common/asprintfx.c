#include "asprintfx.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

char *asprintfx(const char *fmt,  ...)
{
	char *dest;
	va_list ap;
	int len;

	assert(fmt != NULL);

	dest = NULL;

	va_start(ap, fmt);
	len = vsnprintf(dest, 0, fmt, ap);

	dest = (char*)malloc(len + 1);
	vsprintf(dest, fmt, ap);

	va_end(ap);

	return dest;
}

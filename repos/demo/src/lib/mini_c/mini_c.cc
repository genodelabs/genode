/*
 * \brief  Mini C dummy functions
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

#define NOT_IMPLEMENTED Genode::log(__func__, " not implemented")

extern "C" {

#include <stdio.h>

int sprintf(char *, const char *, ...)
	{ NOT_IMPLEMENTED; return 0; }
int printf(const char *, ...)
	{ NOT_IMPLEMENTED; return 0; }
FILE *fopen(const char *, const char *)
	{ NOT_IMPLEMENTED; return 0; }
FILE *fdopen(int, const char *)
	{ NOT_IMPLEMENTED; return 0; }
int fclose(FILE *)
	{ NOT_IMPLEMENTED; return 0; }
int fprintf(FILE *, const char *, ...)
	{ NOT_IMPLEMENTED; return 0; }
size_t fwrite(const void *, size_t, size_t, FILE *)
	{ NOT_IMPLEMENTED; return 0; }
unsigned fread(void *, unsigned, unsigned, FILE *)
	{ NOT_IMPLEMENTED; return 0; }
int fputc(int, FILE *)
	{ NOT_IMPLEMENTED; return 0; }
int fflush(FILE *)
	{ NOT_IMPLEMENTED; return 0; }
int fseek(FILE *, long, int)
	{ NOT_IMPLEMENTED; return 0; }
long ftell(FILE *)
	{ NOT_IMPLEMENTED; return 0; }
void clearerr(FILE *)
	{ NOT_IMPLEMENTED; }
int ferror(FILE *)
	{ NOT_IMPLEMENTED; return 0; }
int puts(const char *s)
	{ Genode::log("%s", s); return 1; }
int putchar(int c)
	{ Genode::log("%c", c); return c; }

#include <stdlib.h>

int abs(int j)
{
	return j < 0 ? -j : j;
}

#include <string.h>

char *strcpy(char *dest, const char *src)
	{ return strncpy(dest, src, ~0); }
char *strcat(char *, const char *)
	{ NOT_IMPLEMENTED; return 0; }

static inline size_t min(size_t v1, size_t v2) { return v1 < v2 ? v1 : v2; }

char *strncpy(char *dst, const char *src, size_t n)
{
	n = min(n, strlen(src) + 1);
	memcpy(dst, src, n);
	if (n > 0) dst[n - 1] = 0;
	return dst;
}

char *strerror(int) { NOT_IMPLEMENTED; return 0; }

} /* extern "C" */

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

#include <stdio.h>

#ifdef GENODE_RELEASE
#define printf(...)
#endif /* GENODE_RELEASE */

int sprintf(char *str, const char *format, ...)
	{ printf("%s: not implemented\n", __func__); return 0; }
FILE *fopen(const char *path, const char *mode)
	{ printf("%s: not implemented\n", __func__); return 0; }
FILE *fdopen(int fildes, const char *mode)
	{ printf("%s: not implemented\n", __func__); return 0; }
int fclose(FILE *fp)
	{ printf("%s: not implemented\n", __func__); return 0; }
int fprintf(FILE *stream, const char *format, ...)
	{ printf("%s: not implemented\n", __func__); return 0; }
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
	{ printf("%s: not implemented\n", __func__); return 0; }
unsigned fread(void *ptr, unsigned size, unsigned nmemb, FILE *stream)
	{ printf("%s: not implemented\n", __func__); return 0; }
int fputc(int c, FILE *stream)
	{ printf("%s: not implemented\n", __func__); return 0; }
int fflush(FILE *stream)
	{ printf("%s: not implemented\n", __func__); return 0; }
int fseek(FILE *stream, long offset, int whence)
	{ printf("%s: not implemented\n", __func__); return 0; }
long ftell(FILE *stream)
	{ printf("%s: not implemented\n", __func__); return 0; }
void clearerr(FILE *stream)
	{ printf("%s: not implemented\n", __func__); }
int ferror(FILE *stream)
	{ printf("%s: not implemented\n", __func__); return 0; }
int puts(const char *s)
	{ printf("%s", s); return 1; }
int putchar(int c)
	{ printf("%c", c); return c; }

#include <stdlib.h>

int abs(int j)
{
	return j < 0 ? -j : j;
}

/* in alloc_env_backend.cc
void *calloc(unsigned nmemb, unsigned size)
	{ printf("%s: not implemented\n", __func__); return 0; }
*/

#include <string.h>

/* in base/cxx
void *memcpy(void *dest, const void *src, unsigned n);
*/
char *strcpy(char *dest, const char *src)
	{ return strncpy(dest, src, ~0); }
char *strcat(char *dest, const char *src)
	{ printf("%s: not implemented\n", __func__); return 0; }

inline size_t min(size_t v1, size_t v2) { return v1 < v2 ? v1 : v2; }

char *strncpy(char *dst, const char *src, size_t n)
{
	n = min(n, strlen(src) + 1);
	memcpy(dst, src, n);
	if (n > 0) dst[n - 1] = 0;
	return dst;
}

char *strerror(int errnum)
	{ printf("%s: not implemented\n", __func__); return 0; }

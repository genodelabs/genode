/*
 * \brief  string.h prototypes/definitions required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define alloca(size) __builtin_alloca ((size))

/*
 * prototypes
 */
#ifdef __cplusplus
extern "C" {
#endif
void bzero(void *s, size_t n);

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

char *strncpy(char *dst, const char *src, size_t n);
size_t strlen(const char *s);
#ifdef __cplusplus
}
#endif


/*
 * dummies
 */
static inline
const char *strerror(int errnum) { (void)errnum; return ""; }


static inline
size_t strspn(const char *s, const char *accept)
{
	extern int debug;
	if (debug)
		printf("Warning: %s called\n", __func__);
	return 0;
}

static inline
size_t strcspn(const char *s, const char *reject)
{
	extern int debug;
	if (debug)
		printf("Warning: %s called\n", __func__);
	return 0;
}

static inline
char *strchr(const char *s, int c)
{
	extern int debug;
	if (debug)
		printf("Warning: %s called\n", __func__);
	return NULL;
}

static inline
char *strrchr(const char *s, int c)
{
	extern int debug;
	if (debug)
		printf("Warning: %s called\n", __func__);
	return NULL;
}


/*
 * inlines
 */
static inline
char *
strcpy(char *to, const char *from)
{
	char *save = to;

	for (; (*to = *from); ++from, ++to);
  return(save);
}

static inline
char *
strdup(const char *str)
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if ((copy = (char*)malloc(len)) == NULL)
		return (NULL);
	memcpy(copy, str, len);
	return (copy);
}

static inline
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

#endif // _STRING_H_

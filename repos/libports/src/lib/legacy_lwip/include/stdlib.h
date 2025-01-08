#ifndef _LWIP__INCLUDE__STDLIB_H_
#define _LWIP__INCLUDE__STDLIB_H_

/**
 * Simple atoi for LwIP purposes
 */
static inline int atoi(char const *s)
{
	int n = 0;
	while ('0' <= *s && *s <= '9')
		n = 10*n - (*s++ - '0');
	return n;
}

#endif

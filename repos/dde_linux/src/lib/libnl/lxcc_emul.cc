/**
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-07-28
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/log.h>
#include <util/string.h>


/**************
 ** stdlib.h **
 **************/

static char getenv_NLDBG[]          = "1";
static char getenv_HZ[]             = "100";
static char getenv_TICKS_PER_USEC[] = "10000";


extern "C" char *getenv(const char *name)
{
	if (Genode::strcmp(name, "NLDBG") == 0)          return getenv_NLDBG;
	if (Genode::strcmp(name, "HZ") == 0)             return getenv_HZ;
	if (Genode::strcmp(name, "TICKS_PER_USEC") == 0) return getenv_TICKS_PER_USEC;

	return nullptr;
}


#if 0
#include <base/env.h>
#include <base/log.h>
#include <base/snprintf.h>
#include <dataspace/client.h>
#include <timer_session/connection.h>
#include <rom_session/connection.h>
#include <util/string.h>

#include <lx_emul.h>


extern "C" {

/*************
 ** errno.h **
 *************/

int errno;


/*************
 ** stdio.h **
 *************/

FILE *stdout;
FILE *stderr;


void *malloc(size_t size)
{
	/* align on pointer size */
	size = size + ((sizeof(Genode::addr_t)-1) & ~(sizeof(Genode::addr_t)-1));

	size_t rsize = size + sizeof (Genode::addr_t);

	void *addr = 0;
	if (!Genode::env()->heap()->alloc(size, &addr))
		return 0;

	*(Genode::addr_t*)addr = rsize;
	return ((Genode::addr_t*)addr) + 1;
}


void *calloc(size_t nmemb, size_t size)
{
#define MUL_NO_OVERFLOW (1UL << (sizeof(size_t) * 4))
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
		nmemb > 0 && SIZE_MAX / nmemb < size) {
		return NULL;
	}

	size *= nmemb;

	void *addr = malloc(size);
	Genode::memset(addr, 0, size);
	return addr;
}


void free(void *ptr)
{
	if (!ptr)
		return;

	Genode::addr_t *addr = ((Genode::addr_t*)ptr) - 1;
	Genode::env()->heap()->free(addr, *addr);
}

void *realloc(void *ptr, Genode::size_t size)
{
	if (!size)
		free(ptr);

	void   *n = malloc(size);
	size_t  s = *((size_t *)ptr - 1);

	Genode::memcpy(n, ptr, Genode::min(s, size));

	free(ptr);

	return n;
}


/**************
 ** stdlib.h **
 **************/

static char getenv_HZ[]             = "100";
static char getenv_TICKS_PER_USEC[] = "10000";

char *getenv(const char *name)
{
	/* these values must match the ones of lx_emul wifi */

	if (Genode::strcmp(name, "HZ") == 0)             return getenv_HZ;
	if (Genode::strcmp(name, "TICKS_PER_USEC") == 0) return getenv_TICKS_PER_USEC;

	return nullptr;
}


long int strtol(const char *nptr, char **endptr, int base)
{
	long res = 0;
	if (base != 0 && base != 10) {
		Genode::error("strtol: base of ", base, " is not supported");
		return 0;
	}
	Genode::ascii_to(nptr, res);
	return res;
}


double strtod(const char *nptr, char **endptr)
{
	double res = 0;
	Genode::ascii_to(nptr, res);
	return res;
}


/********************
 ** linux/string.h **
 ********************/

size_t strcspn(const char *s, const char *reject)
{
	for (char const *p = s; *p; p++) {
		char c = *p;

		for (char const *r = reject; *r; r++) {
			char d = *r;
			if (c == d)
				return (p - 1 - s);
		}
	}
	return 0;
}


char *strdup(const char *s)
{
	size_t len = strlen(s);

	char *p = (char *) malloc(len + 1);

	return strncpy(p, s, len + 1);
}


size_t strlen(const char *s)
{
	return Genode::strlen(s);
}


int strcasecmp(const char* s1, const char *s2)
{
	return Genode::strcmp(s1, s2);
}


int strcmp(const char* s1, const char *s2)
{
	return Genode::strcmp(s1, s2);
}


int strncmp(const char *s1, const char *s2, size_t len)
{
	return Genode::strcmp(s1, s2, len);
}


char *strchr(const char *p, int ch)
{
	char c;
	c = ch;
	for (;; ++p) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			break;
	}

	return 0;
}


void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = reinterpret_cast<const unsigned char*>(s);
	while (n-- != 0) {
		if ((unsigned char)c == *p++) {
			return (void *)(p - 1);
		}
	}
	return NULL;
}


char *strnchr(const char *p, size_t count, int ch)
{
	char c;
	c = ch;
	for (; count; ++p, count--) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			break;
	}

	return 0;
}


char *strncat(char *dst, const char *src, size_t n)
{
	char *p = dst;
	while (*p++) ;

	while ((*p = *src) && (n-- > 0)) {
		++src;
		++p;
	}

	return dst;
}


char *strcpy(char *dst, const char *src)
{
	char *p = dst;

	while ((*dst = *src)) {
		++src;
		++dst;
	}

	return p;
}


char *strncpy(char *dst, const char* src, size_t n)
{
	return Genode::strncpy(dst, src, n);
}


int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list list;

	va_start(list, format);
	Genode::String_console sc(str, size);
	sc.vprintf(format, list);
	va_end(list);

	return sc.len();
}


int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
	Genode::String_console sc(str, size);
	sc.vprintf(format, args);

	return sc.len();
}


int asprintf(char **strp, const char *fmt, ...)
{
	/* XXX for now, let's hope strings are not getting longer than 256 bytes */
	enum { MAX_STRING_LENGTH = 256 };
	char *p = (char*)malloc(MAX_STRING_LENGTH);
	if (!p)
		return -1;

	va_list args;

	va_start(args, fmt);
	Genode::String_console sc(p, MAX_STRING_LENGTH);
	sc.vprintf(fmt, args);
	va_end(args);

	return strlen(p);
}


/**************
 ** unistd.h **
 **************/

int getpagesize(void) {
	return 4096; }


pid_t getpid(void) {
	return 42; }


/************
 ** time.h **
 ************/

extern unsigned long jiffies;

time_t time(time_t *t)
{
	return jiffies;
}

} /* extern "C" */
#endif

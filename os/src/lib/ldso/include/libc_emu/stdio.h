/*
 * \brief  stdio.h prototypes/definitions required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <ldso_types.h>

//implementations
#ifdef __cplusplus
extern "C" {
#endif
int printf(const char *format, ...);
int vprintf(const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int putc(int c, int *stream);
int putchar(int c);
int vfprintf(int *stream, const char *format, va_list ap);
#ifdef __cplusplus
}
#endif

//dummies
static  inline int fflush(int *stream) {(void)stream; return 0;}
static inline void exit(int status) __attribute__((noreturn));

static inline void exit(int status)
{
	while(1) ;
}

#endif //STDIO_H


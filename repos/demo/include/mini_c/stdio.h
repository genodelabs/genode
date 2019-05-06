/*
 * \brief  Mini C standard I/O
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__MINI_C__STDIO_H_
#define _INCLUDE__MINI_C__STDIO_H_

#include <stdarg.h>
#include <stddef.h>

#define FILE void

#define EOF (-1)

int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fildes, const char *mode);
int fclose(FILE *fp);
int fprintf(FILE *stream, const char *format, ...);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
unsigned fread(void *ptr, unsigned size, unsigned nmemb, FILE *stream);
int fputc(int c, FILE *stream);
int fflush(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void clearerr(FILE *stream);
int ferror(FILE *stream);

#endif /* _INCLUDE__MINI_C__STDIO_H_ */

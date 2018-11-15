/*
 * \brief  Minimal C library for libgcov
 * \author Christian Prochaska
 * \date   2018-11-16
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef STDIO_H
#define STDIO_H

#include <sys/types.h>

#define SEEK_SET 0

struct FILE;
typedef struct FILE FILE;

extern FILE *stderr;

int fclose(FILE *stream);
FILE *fopen(const char *path, const char *mode);
int fprintf(FILE *stream, const char *format, ...);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
void setbuf(FILE *stream, char *buf);
int vfprintf(FILE *stream, const char *format, va_list ap);

#endif

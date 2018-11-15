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

#ifndef STRING_H
#define STRING_H

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
char *strcpy(char *dest, const char *src);
size_t strlen(const char *s);

#endif

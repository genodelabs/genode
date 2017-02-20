/*
 * \brief  Mini C standard library
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__MINI_C__STDLIB_H_
#define _INCLUDE__MINI_C__STDLIB_H_

#include <stddef.h>

int abs(int j);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);

void abort(void);
long int strtol(const char *nptr, char **endptr, int base);
long atol(const char *nptr);
double strtod(const char *nptr, char **endptr);

#endif /* _INCLUDE__MINI_C__STDLIB_H_ */

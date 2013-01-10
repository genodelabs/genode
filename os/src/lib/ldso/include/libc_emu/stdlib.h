/*
 * \brief  stdlib.h prototypes/definitions required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef STDLIB_H
#define STDLIB_H

#include <ldso_types.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
char *getenv(const char *);

#ifdef __cplusplus
}
#endif

// gcc  built-in
void *alloca(size_t size);

static inline
void abort(void) {}

static inline
int unsetenv(const char *name)
{
	return 0;
}
#endif //STDLIB_H

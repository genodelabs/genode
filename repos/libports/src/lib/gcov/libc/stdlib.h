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

#ifndef STDLIB_H
#define STDLIB_H

void abort(void);
void atexit(void (*func)(void));
int atoi(const char *nptr);
void free(void *ptr);
char *getenv(const char *name);
void *malloc(size_t size);

#endif

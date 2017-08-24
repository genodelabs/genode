/**
 * \brief  Stdio definitions
 * \author Sebastian Sumpf
 * \date   2017-08-24
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>

typedef unsigned long long uint64_t;

int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);

#endif /* _STDIO_H_ */

/**
 * \brief  String definitions
 * \author Sebastian Sumpf
 * \date   2017-08-24
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STRING_H_
#define _STRING_H_

#include <stdint.h>

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

#endif /* _STRING_H_ */

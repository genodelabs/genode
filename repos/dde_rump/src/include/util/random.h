/**
 * \brief  Overwritable backend-random methods
 * \author Sebastian Sumpf
 * \date   2015-02-13
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__RANDOM_H_
#define _INCLUDE__UTIL__RANDOM_H_

#include <base/stdint.h>

int rumpuser_getrandom_backend(void *buf, Genode::size_t buflen, int flags, Genode::size_t *retp);

#endif /* _INCLUDE__UTIL__RANDOM_H_ */

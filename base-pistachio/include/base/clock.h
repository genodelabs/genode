/*
 * \brief  Timer interface
 * \author Julian Stecklina
 * \date   2007-12-30
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CLOCK_H_
#define _INCLUDE__BASE__CLOCK_H_

#include <stdint.h>

namespace Genode {

	typedef uint64_t cycles_t;

	/**
	 * Returns the clock resolution in nanoseconds
	 */
	uint64_t clock_resolution();

	/**
	 * Return the current time as nanoseconds
	 *
	 * The base of this value is unspecified, but the value should not
	 * wrap in at least several years.
	 */
	uint64_t get_time();
}

#endif /* _INCLUDE__BASE__CLOCK_H_ */

/*
 * \brief  Integer types
 * \author Christian Helmuth
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__STDINT_H_
#define _INCLUDE__BASE__STDINT_H_

/* fixed-width integer types */
#include <base/fixed_stdint.h>

namespace Genode {

	/**
	 * Integer type for non-negative size values
	 */
	typedef unsigned long size_t;

	/**
	 * Integer type for memory addresses
	 */
	typedef unsigned long addr_t;

	/**
	 * Integer type for memory offset values
	 */
	typedef long          off_t;

	/**
	 * Integer type corresponding to a machine register
	 */
	typedef unsigned long umword_t;
}

#endif /* _INCLUDE__BASE__STDINT_H_ */

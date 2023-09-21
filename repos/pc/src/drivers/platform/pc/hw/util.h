/*
 * \brief   Common utilities
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-01-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__UTIL_H_
#define _SRC__LIB__HW__UTIL_H_

namespace Hw {

	using Genode::addr_t;
	using Genode::size_t;

	/**
	 * Return an address rounded down to a specific alignment
	 *
	 * \param addr         original address
	 * \param alignm_log2  log2 of the required alignment
	 */
	constexpr addr_t trunc(addr_t addr, addr_t alignm_log2) {
		return (addr >> alignm_log2) << alignm_log2; }

	/**
	 * Return wether a pointer fullfills an alignment
	 *
	 * \param p            pointer
	 * \param alignm_log2  log2 of the required alignment
	 */
	inline bool aligned(void * const p, addr_t alignm_log2) {
		return (addr_t)p == trunc((addr_t)p, alignm_log2); }
}

#endif /* _SRC__LIB__HW__UTIL_H_ */

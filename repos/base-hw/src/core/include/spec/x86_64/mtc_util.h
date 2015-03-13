/*
 * \brief  x86 utility functions
 * \author Reto Buerki
 * \date   2015-02-27
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MTC_UTIL_H_
#define _MTC_UTIL_H_

#include <base/stdint.h>

extern int _mt_begin;

namespace Genode
{
	/**
	 * Return virtual mtc address of the given label for the specified
	 * virtual mode transition base.
	 *
	 * \param virt_base  virtual base of the mode transition pages
	 */
	static addr_t _virt_mtc_addr(addr_t const virt_base, addr_t const label)
	{
		addr_t const phys_base = (addr_t)&_mt_begin;
		return virt_base + (label - phys_base);
	}
}

#endif /* _MTC_UTIL_H_ */

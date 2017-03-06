/*
 * \brief  Type-safe, fine-grained access to a continuous MMIO region
 * \author Martin stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__MMIO_H_
#define _INCLUDE__UTIL__MMIO_H_

/* Genode includes */
#include <util/register_set.h>

namespace Genode {

	class Mmio_plain_access;
	class Mmio;
}

/**
 * Plain access implementation for MMIO
 */
class Genode::Mmio_plain_access
{
	friend Register_set_plain_access;

	private:

		addr_t const _base;

		/**
		 * Write '_ACCESS_T' typed 'value' to MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline void _write(off_t const offset, ACCESS_T const value)
		{
			addr_t const dst = _base + offset;
			*(ACCESS_T volatile *)dst = value;
		}

		/**
		 * Read '_ACCESS_T' typed from MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline ACCESS_T _read(off_t const &offset) const
		{
			addr_t const dst = _base + offset;
			ACCESS_T const value = *(ACCESS_T volatile *)dst;
			return value;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param base  base address of targeted MMIO region
		 */
		Mmio_plain_access(addr_t const base) : _base(base) { }

		addr_t base() const { return _base; }
};


/**
 * Type-safe, fine-grained access to a continuous MMIO region
 *
 * For further details refer to the documentation of the 'Register_set' class.
 */
struct Genode::Mmio : Mmio_plain_access, Register_set<Mmio_plain_access>
{
	/**
	 * Constructor
	 *
	 * \param base  base address of targeted MMIO region
	 */
	Mmio(addr_t const base)
	:
		Mmio_plain_access(base),
		Register_set(*static_cast<Mmio_plain_access *>(this)) { }
};

#endif /* _INCLUDE__UTIL__MMIO_H_ */

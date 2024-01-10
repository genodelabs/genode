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
#include <base/log.h>
#include <util/string.h>
#include <util/register_set.h>

namespace Genode {

	class Mmio_plain_access;
	template <size_t> class Mmio;
}

/**
 * Plain access implementation for MMIO
 */
class Genode::Mmio_plain_access
{
	friend Register_set_plain_access;

	private:

		Byte_range_ptr const _range;

		/**
		 * Write 'ACCESS_T' typed 'value' to MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline void _write(off_t const offset, ACCESS_T const value)
		{
			*(ACCESS_T volatile *)(_range.start + offset) = value;
		}

		/**
		 * Read 'ACCESS_T' typed from MMIO base + 'offset'
		 */
		template <typename ACCESS_T>
		inline ACCESS_T _read(off_t const &offset) const
		{
			return *(ACCESS_T volatile *)(_range.start + offset);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param base  base address of targeted MMIO region
		 */
		Mmio_plain_access(Byte_range_ptr const &range) : _range(range.start, range.num_bytes) { }

		Byte_range_ptr range_at(off_t offset) const
		{
			return {_range.start + offset, _range.num_bytes - offset};
		}

		Byte_range_ptr range() const { return range_at(0); }

		addr_t base() const { return (addr_t)range().start; }
};


/**
 * Type-safe, fine-grained access to a continuous MMIO region
 *
 * For further details refer to the documentation of the 'Register_set' class.
 */
template <Genode::size_t MMIO_SIZE>
struct Genode::Mmio : Mmio_plain_access, Register_set<Mmio_plain_access, MMIO_SIZE>
{
	static constexpr size_t SIZE = MMIO_SIZE;

	class Range_violation : Exception { };

	/**
	 * Constructor
	 *
	 * \param range  byte range of targeted MMIO region
	 */
	Mmio(Byte_range_ptr const &range)
	:
		Mmio_plain_access(range),
		Register_set<Mmio_plain_access, SIZE>(*static_cast<Mmio_plain_access *>(this))
	{
		if (range.num_bytes < SIZE) {
			error("MMIO range is unexpectedly too small");
			throw Range_violation { };
		}
	}
};

#endif /* _INCLUDE__UTIL__MMIO_H_ */

/*
 * \brief  Extension of MMIO framework with const read-only MMIO regions
 * \author Stefan Kalkowski
 * \date   2024-02-21
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _USB_BLOCK__MMIO_H_
#define _USB_BLOCK__MMIO_H_

#include <util/mmio.h>

namespace Genode {
	class Const_mmio_plain_access;
	template <size_t> class Const_mmio;
};


/**
 * Plain access implementation for MMIO
 */
class Genode::Const_mmio_plain_access
{
	friend Register_set_plain_access;

	private:

		Const_byte_range_ptr const _range;

		template <typename ACCESS_T>
		inline ACCESS_T _read(off_t const &offset) const {
			return *(ACCESS_T volatile *)(_range.start + offset); }

	public:

		Const_mmio_plain_access(Const_byte_range_ptr const &range)
		: _range(range.start, range.num_bytes) { }

		Const_byte_range_ptr range_at(off_t offset) const {
			return {_range.start + offset, _range.num_bytes - offset}; }

		Const_byte_range_ptr range() const { return range_at(0); }

		addr_t base() const { return (addr_t)range().start; }
};


template <Genode::size_t MMIO_SIZE>
struct Genode::Const_mmio : Const_mmio_plain_access,
                            Register_set<Const_mmio_plain_access, MMIO_SIZE>
{
	static constexpr size_t SIZE = MMIO_SIZE;

	class Range_violation : Exception { };

	Const_mmio(Const_byte_range_ptr const &range)
	:
		Const_mmio_plain_access(range),
		Register_set<Const_mmio_plain_access, SIZE>(*static_cast<Const_mmio_plain_access *>(this))
	{
		if (range.num_bytes < SIZE) {
			error("MMIO range is unexpectedly too small");
			throw Range_violation { };
		}
	}
};

#endif /* _USB_BLOCK__MMIO_H_ */

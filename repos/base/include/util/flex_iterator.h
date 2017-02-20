/*
 * \brief  Flexpage iterator
 * \author Alexander Boettcher
 * \date   2013-01-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__FLEX_ITERATOR_H_
#define _INCLUDE__UTIL__FLEX_ITERATOR_H_

#include <base/stdint.h>
#include <util/misc_math.h>

namespace Genode {

	struct Flexpage;
	class Flexpage_iterator;
}


struct Genode::Flexpage
{
	addr_t addr;
	addr_t hotspot;
	size_t log2_order;

	Flexpage() : addr(~0UL), hotspot(0), log2_order(0) { }

	Flexpage(addr_t a, addr_t h, size_t o)
	: addr(a), hotspot(h), log2_order(o) { }

	bool valid() { return addr != ~0UL; }
};


class Genode::Flexpage_iterator
{
	private:

		addr_t _src_start, _src_size;
		addr_t _dst_start, _dst_size;
		addr_t _hotspot, _offset;

		/**
		 * Find least significant set bit in value
		 */
		inline addr_t
		lsb_bit(addr_t const scan)
		{
			if (scan == 0)
				return ~0UL;
			return __builtin_ctzl(scan);
		}

	public:

		Flexpage_iterator() { }

		Flexpage_iterator(addr_t src_start, size_t src_size,
		                  addr_t dst_start, size_t dst_size,
		                  addr_t hotspot)
		:
			_src_start(src_start), _src_size(src_size),
			_dst_start(dst_start), _dst_size(dst_size),
			_hotspot(hotspot), _offset(0)
		{ }

		Flexpage page()
		{
			size_t const size = min (_src_size, _dst_size);

			addr_t const from_end = _src_start + size;
			addr_t const to_end   = _dst_start + size;

			if (_offset >= size)
				return Flexpage();

			addr_t const from_curr = _src_start + _offset;
			addr_t const to_curr   = _dst_start + _offset;

			/*
			 * The common alignment corresponds to the number of least
			 * significant zero bits in both addresses.
			 */
			addr_t const common_bits = from_curr | to_curr;

			/* find least set bit in common bits */
			size_t order = lsb_bit(common_bits);
			size_t max = (order == ~0UL) ? ~0UL : (1UL << order);

			/* look if it still fits into both 'src' and 'dst' ranges */
			if ((from_end - from_curr) < max) {
				order = log2(from_end - from_curr);
				order = (order == ~0UL) ? 12 : order;
				max   = 1UL << order;
			}

			if ((to_end - to_curr) < max) {
				order = log2(to_end - to_curr);
				order = (order == ~0UL) ? 12 : order;
			}

			/* advance offset by current flexpage size */
			_offset += (1UL << order);

			return Flexpage(from_curr, _hotspot + _offset - (1UL << order), order);
		}
};

#endif /* _INCLUDE__UTIL__FLEX_ITERATOR_H_ */

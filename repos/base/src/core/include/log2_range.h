/*
 * \brief  Utility for dealing with log2 alignment constraints
 * \author Norman Feske
 * \date   2023-06-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__LOG2_RANGE_H_
#define _CORE__INCLUDE__LOG2_RANGE_H_

/* core includes */
#include <util.h>
#include <addr_range.h>

namespace Core { struct Log2_range; }


struct Core::Log2_range
{
	Addr hotspot { 0 };
	Addr base    { 0 };
	Log2 size    { 0 };

	bool valid() const { return size.log2 >= get_page_size_log2(); }

	static constexpr Log2 UNCONSTRAINED = { uint8_t(~0) };

	/**
	 * Default constructor, constructs invalid range
	 */
	Log2_range() { }

	/**
	 * Constructor, hotspot area spans the maximum address-space size
	 */
	Log2_range(Addr hotspot) : hotspot(hotspot), size(UNCONSTRAINED) { }

	/**
	 * Constrain range to specified region
	 */
	Log2_range constrained(Addr_range region) const
	{
		addr_t const upper_bound = (size.log2 == UNCONSTRAINED.log2)
		                         ? ~0UL : (base.value + (1UL << size.log2) - 1);

		/*
		 * Find flexpage around hotspot that lies within the specified region.
		 *
		 * Start with a 'size' of one less than the minimal page size.
		 * If the specified constraint conflicts with the existing range,
		 * the loop breaks at the first iteration and we can check for this
		 * condition after the loop.
		 */
		Log2_range result { hotspot };
		result.size = { get_page_size_log2() - 1 };

		for (uint8_t try_size_log2 = get_page_size_log2();
		     try_size_log2 < sizeof(addr_t)*8 ; try_size_log2++) {

			addr_t const fpage_mask = ~((1UL << try_size_log2) - 1);
			addr_t const try_base = hotspot.value & fpage_mask;

			/* check lower bound of existing range */
			if (try_base < base.value)
				break;

			/* check against upper bound of existing range */
			if (try_base + (1UL << try_size_log2) - 1 > upper_bound)
				break;

			/* check against lower bound of region */
			if (try_base < region.start)
				break;

			/* check against upper bound of region */
			if (try_base + (1UL << try_size_log2) - 1 > region.end)
				break;

			/* flexpage is compatible with the range, use it */
			result.size = { try_size_log2 };
			result.base = { try_base };
		}

		return result.valid() ? result : Log2_range { };
	}

	/**
	 * Constrain range around hotspot to specified log2 size
	 */
	Log2_range constrained(Log2 value) const
	{
		Log2_range result = *this;

		if (value.log2 >= size.log2)
			return result;

		result.base = { hotspot.value & ~((1UL << value.log2) - 1) };
		result.size = value;
		return result;
	}

	/**
	 * Determine common log2 size compatible with both ranges
	 */
	static Log2 common_log2(Log2_range const &r1, Log2_range const &r2)
	{
		/*
		 * We have to make sure that the offset of hotspot
		 * relative to the flexpage base is the same for both ranges.
		 * This condition is met by the flexpage size equal to the number
		 * of common least-significant bits of both offsets.
		 */
		size_t const diff = (r1.hotspot.value - r1.base.value)
		                  ^ (r2.hotspot.value - r2.base.value);

		/*
		 * Find highest clear bit in 'diff', starting from the least
		 * significant candidate. We can skip all bits lower then
		 * 'get_page_size_log2()' because they are not relevant as
		 * flexpage size (and are always zero).
		 */
		uint8_t n = get_page_size_log2();
		size_t const min_size_log2 = min(r1.size.log2, r2.size.log2);
		for (; n < min_size_log2 && !(diff & (1UL << n)); n++);

		return Log2 { n };
	}
};

#endif /* _CORE__INCLUDE__LOG2_RANGE_H_ */

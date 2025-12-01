/*
 * \brief  Commonly used math functions
 * \author Norman Feske
 * \date   2006-04-12
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__MISC_MATH_H_
#define _INCLUDE__UTIL__MISC_MATH_H_

#include <base/stdint.h>

namespace Genode {

	template <typename T1, typename T2>
	static constexpr T1 max(T1 v1, T2 v2) { return v1 > v2 ? v1 : v2; }

	template <typename T1, typename T2>
	static constexpr T1 min(T1 v1, T2 v2) { return v1 < v2 ? v1 : v2; }


	/**
	 * Alignment argument specified as a power of two
	 */
	struct Align { uint8_t log2; };


	/*
	 * Alignment to the power of two
	 */
	template <typename T>
	static constexpr T _align_mask(Align a)   { return ~T((T(1) << a.log2) - 1u); }

	template <typename T>
	static constexpr T _align_offset(Align a) { return  T((T(1) << a.log2) - 1u); }

	template <typename T>
	static constexpr T align_addr(T addr, Align align) {
		return (addr + _align_offset<T>(align)) & _align_mask<T>(align); }

	template <typename T>
	static constexpr bool aligned(T value, Align align) {
		return (_align_offset<T>(align) & value) == 0; }


	/**
	 * LOG2
	 *
	 * Scan for most significant set bit
	 */
	template <typename T>
	static constexpr uint8_t log2(T value, uint8_t result_if_value_is_zero)
	{
		if (value)
			for (uint8_t i = 8*sizeof(value); i > 0; i--)
				if ((T(1) << (i - 1)) & value)
					return i - 1;

		return result_if_value_is_zero;
	}


	/**
	 * Align value to next machine-word boundary
	 */
	template <typename T>
	static constexpr T align_natural(T value)
	{
		T mask = sizeof(long) - 1;
		return (T)(value + mask) & (T)(~mask);
	}


	/**
	 * Alignment for heap allocation
	 */
	static constexpr Align AT_16_BYTES { .log2 = 4 };

	/**
	 * Alignment at virtual-memory page boundary
	 */
	static constexpr Align AT_PAGE { .log2 = 12 };

	/**
	 * Alignment at machine-word size
	 */
	static constexpr Align AT_MWORD { .log2 = log2(sizeof(addr_t), 0u) };
}

#endif /* _INCLUDE__UTIL__MISC_MATH_H_ */

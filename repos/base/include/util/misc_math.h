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

namespace Genode {

	template <typename T1, typename T2>
	static constexpr T1 max(T1 v1, T2 v2) { return v1 > v2 ? v1 : v2; }

	template <typename T1, typename T2>
	static constexpr T1 min(T1 v1, T2 v2) { return v1 < v2 ? v1 : v2; }

	template <typename T>
	static constexpr T abs(T value) { return value >= 0 ? value : -value; }


	/*
	 * Alignment to the power of two
	 */
	template <typename T>
	static constexpr T _align_mask(T align) {
		return ~(((T)1 << align) - (T)1); }

	template <typename T>
	static constexpr T _align_offset(T align) {
		return   ((T)1 << align) - (T)1;  }

	template <typename T>
	static constexpr T align_addr(T addr, int align) {
		return (addr + _align_offset((T)align)) & _align_mask((T)align); }

	template <typename T>
	static constexpr bool aligned(T value, unsigned align_log2) {
		return (_align_offset(align_log2) & value) == 0; }


	/**
	 * LOG2
	 *
	 * Scan for most-significant set bit.
	 */
	template <typename T>
	static inline T log2(T value)
	{
		if (!value) return -1;
		for (int i = 8 * sizeof(value) - 1; i >= 0; --i)
			if (((T)1 << i) & value) return i;

		return -1;
	}


	/**
	 * Align value to next machine-word boundary
	 */
	template <typename T>
	inline T align_natural(T value)
	{
		T mask = sizeof(long) - 1;
		return (value + mask) & ~mask;
	}
}

#endif /* _INCLUDE__UTIL__MISC_MATH_H_ */

/*
 * \brief  Core-internal utilities
 * \author Martin Stein
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__MATH_H_
#define _INCLUDE__UTIL__MATH_H_

namespace Math
{
	template <typename T>
	inline T round_up(T v, T rounding_log2);

	template <typename T>
	inline T round_down(T v, T rounding_log2);
}


template <typename T>
T Math::round_up(T v, T rounding_log2)
{ 
	return round_down(v + (1<<rounding_log2) - 1, rounding_log2) ; 
}


template <typename T>
T Math::round_down(T v, T rounding_log2)
{ 
	return v & ~((1 << rounding_log2) - 1); 
}

#endif /* _INCLUDE__UTIL__MATH_H_ */

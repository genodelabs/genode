/*
 * \brief  Commonly used math functions
 * \author Norman Feske
 * \date   2006-04-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_GFX__MISCMATH_H_
#define _INCLUDE__NITPICKER_GFX__MISCMATH_H_

template <typename T>
T max(T v1, T v2) { return v1 > v2 ? v1 : v2; }

template <typename T>
T min(T v1, T v2) { return v1 < v2 ? v1 : v2; }

#endif /* _INCLUDE__NITPICKER_GFX__MISCMATH_H_ */

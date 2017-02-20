/*
 * \brief   Square root of integer values
 * \date    2010-09-27
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NANO3D__SQRT_H_
#define _INCLUDE__NANO3D__SQRT_H__

namespace Nano3d {

	/**
	 * Calculate square root of an integer value
	 */
	template <typename T>
	T sqrt(T value)
	{
		/*
		 * Calculate square root using nested intervalls. The range of values
		 * is log(x) with x being the maximum value of type T. We narrow the
		 * result bit by bit starting with the most significant bit.
		 */
		T result = 0;
		for (T i = sizeof(T)*8 / 2; i > 0; i--) {
			T const bit  = i - 1;
			T const test = result + (1 << bit);
			if (test*test <= value)
				result = test;
		}
		return result;
	}
}

#endif /* _INCLUDE__NANO3D__SQRT_H__ */

/*
 * \brief  Utility for checking array bounds
 * \author Norman Feske
 * \date   2011-02-18
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__RANGE_CHECKED_INDEX_H_
#define _NOUX__RANGE_CHECKED_INDEX_H_

namespace Noux {

	class Index_out_of_range { };

	template <typename T>
	struct Range_checked_index
	{
		T value;
		T const max;

		Range_checked_index(T value, T max)
		: value(value), max(max) { }

		T operator++ (int)
		{
			T old_value = value;

			if (++value >= max)
				throw Index_out_of_range();

			return old_value;
		}

		operator T () { return value; }
	};
}

#endif /* _NOUX__RANGE_CHECKED_INDEX_H_ */

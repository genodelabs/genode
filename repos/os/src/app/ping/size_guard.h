/*
 * \brief  Utility to ensure that a size value doesn't exceed a limit
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SIZE_GUARD_H_
#define _SIZE_GUARD_H_

/* Genode includes */
#include <base/stdint.h>

template <typename EXCEPTION>
class Size_guard
{
	private:

		Genode::size_t       _curr { 0 };
		Genode::size_t const _max;

	public:

		Size_guard(Genode::size_t max) : _max(max) { }

		void add(Genode::size_t size)
		{
			Genode::size_t const new_size = _curr + size;
			if (new_size > _max) { throw EXCEPTION(); }
			_curr = new_size;
		}

		Genode::size_t curr() const { return _curr; }
		Genode::size_t left() const { return _max - _curr; }
};

#endif /* _SIZE_GUARD_H_ */

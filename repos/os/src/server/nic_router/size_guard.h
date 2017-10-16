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

namespace Genode {

	template <size_t MAX, typename EXCEPTION>
	class Size_guard_tpl;
}

template <Genode::size_t MAX, typename EXCEPTION>
class Genode::Size_guard_tpl
{
	private:

		size_t _curr { 0 };

	public:

		void add(size_t size)
		{
			size_t const new_size = _curr + size;
			if (new_size > MAX) { throw EXCEPTION(); }
			_curr = new_size;
		}

		size_t curr() const { return _curr; }
};

#endif /* _SIZE_GUARD_H_ */

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

#ifndef _NET__SIZE_GUARD_H_
#define _NET__SIZE_GUARD_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/exception.h>

namespace Net { class Size_guard; }


class Net::Size_guard
{
	public:

		struct Exceeded : Genode::Exception { };

	private:

		Genode::size_t const _total_size;
		Genode::size_t       _head_size { 0 };
		Genode::size_t       _tail_size { 0 };

		void _consume(Genode::size_t &s1,
		              Genode::size_t &s2,
		              Genode::size_t  s1_consume)
		{
			Genode::size_t const new_s1 = s1 + s1_consume;
			if (new_s1 < s1 || new_s1 > _total_size - s2)
				throw Exceeded();

			s1 = new_s1;
		}

	public:


		Size_guard(Genode::size_t total_size) : _total_size { total_size } { }

		void consume_head(Genode::size_t size) { _consume(_head_size, _tail_size, size); }
		void consume_tail(Genode::size_t size) { _consume(_tail_size, _head_size, size); }


		/***************
		 ** Accessors **
		 ***************/

		Genode::size_t unconsumed() const { return _total_size - _head_size - _tail_size; }
		Genode::size_t tail_size()  const { return _tail_size; }
		Genode::size_t head_size()  const { return _head_size; }
		Genode::size_t total_size() const { return _total_size; }
};

#endif /* _NET__SIZE_GUARD_H_ */

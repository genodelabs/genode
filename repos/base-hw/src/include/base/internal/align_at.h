/*
 * \brief  Utility for aligned object members
 * \author Stefan Kalkowski
 * \date   2017-09-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__ALIGN_AT_H_
#define _INCLUDE__BASE__INTERNAL__ALIGN_AT_H_

#include <util/construct_at.h>
#include <base/stdint.h>

namespace Genode {
	template<typename> class Align_at;
}


template <typename T>
class Genode::Align_at
{
	private:

		static constexpr Genode::size_t ALIGN = alignof(T);

		char _space[sizeof(T) + ALIGN - 1];
		T &  _obj;

		void * _start_addr()
		{
			bool aligned = !((addr_t)_space & (ALIGN - 1));
			return aligned ? (void*)_space
			               : (void*)(((addr_t)_space & ~(ALIGN - 1)) + ALIGN);
		}

	public:

		template <typename... ARGS>
		Align_at(ARGS &&... args)
		: _obj(*construct_at<T>(_start_addr(), args...)) { }

		~Align_at() { _obj.~T(); }

		T       * operator -> ()       { return &_obj; }
		T const * operator -> () const { return &_obj; }
		T       & operator *  ()       { return  _obj; }
		T const & operator *  () const { return  _obj; }
};

#endif /* _INCLUDE__BASE__INTERNAL__ALIGN_AT_H_ */

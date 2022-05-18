/*
 * \brief  Pointer that can be dereferenced only when valid
 * \author Martin Stein
 * \date   2017-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POINTER_H_
#define _POINTER_H_

/* Genode includes */
#include <base/exception.h>

namespace Net { template <typename> class Const_pointer; }

template <typename T>
class Net::Const_pointer
{
	private:

		T const *_ptr { nullptr };

	public:

		struct Invalid : Genode::Exception { };

		Const_pointer() { }

		Const_pointer(T const &ref) : _ptr(&ref) { }

		T const &deref() const
		{
			if (_ptr == nullptr) {
				throw Invalid();
			}
			return *_ptr;
		}

		bool valid() const { return _ptr != nullptr; }
};

#endif /* _POINTER_H_ */

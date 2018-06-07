/*
 * \brief  Pointer that can be dereferenced only when valid
 * \author Martin Stein
 * \date   2016-08-24
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

namespace Net {

	template <typename> class Pointer;
	template <typename> class Const_pointer;
}


template <typename T>
class Net::Pointer
{
	private:

		T *_obj;

	public:

		struct Invalid : Genode::Exception { };

		Pointer() : _obj(nullptr) { }

		Pointer(T &obj) : _obj(&obj) { }

		T &operator () () const
		{
			if (_obj == nullptr)
				throw Invalid();

			return *_obj;
		}

		bool valid() const { return _obj != nullptr; }
};


template <typename T>
class Net::Const_pointer
{
	private:

		T const *_obj;

	public:

		struct Invalid : Genode::Exception { };

		Const_pointer() : _obj(nullptr) { }

		Const_pointer(T const &obj) : _obj(&obj) { }

		T const &operator () () const
		{
			if (_obj == nullptr)
				throw Invalid();

			return *_obj;
		}
};

#endif /* _POINTER_H_ */

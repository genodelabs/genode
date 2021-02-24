/*
 * \brief  Pointer of const object safe against null dereferencing
 * \author Martin Stein
 * \date   2021-04-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CONST_POINTER_H_
#define _CONST_POINTER_H_

/* local includes */
#include <types.h>

namespace File_vault {

	template <typename T>
	class Const_pointer;
}


template <typename OBJECT_TYPE>
class File_vault::Const_pointer
{
	private:

		OBJECT_TYPE const *_object;

	public:

		struct Invalid : Genode::Exception { };

		Const_pointer() : _object { nullptr } { }

		Const_pointer(OBJECT_TYPE const &object) : _object { &object } { }

		OBJECT_TYPE const &object() const
		{
			if (_object == nullptr)
				throw Invalid();

			return *_object;
		}

		bool valid() const { return _object != nullptr; }

		bool operator != (Const_pointer const &other) const
		{
			if (valid() != other.valid()) {
				return true;
			}
			if (valid()) {
				return _object != other._object;
			}
			return false;
		}
};

#endif /* _CONST_POINTER_H_ */

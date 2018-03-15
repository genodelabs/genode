/*
 * \brief  Reference that can be overwritten
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REFERENCE_H_
#define _REFERENCE_H_

namespace Net {

	template <typename> class Reference;
	template <typename> class Const_reference;
}


template <typename T>
class Net::Reference
{
	private:

		T *_obj;

	public:

		Reference(T &obj) : _obj(&obj) { }

		T &operator () () const { return *_obj; }
};


template <typename T>
class Net::Const_reference
{
	private:

		T const *_obj;

	public:

		Const_reference(T const &obj) : _obj(&obj) { }

		T const &operator () () const { return *_obj; }
};

#endif /* _REFERENCE_H_ */

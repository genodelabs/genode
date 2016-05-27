/*
 * \brief  Native capability of base-hw
 * \author Stefan Kalkowski
 * \date   2015-05-15
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_CAPABILITY_H_
#define _INCLUDE__BASE__NATIVE_CAPABILITY_H_

/* Genode includes */
#include <kernel/interface.h>
#include <base/stdint.h>

namespace Genode { class Native_capability; }


class Genode::Native_capability
{
	public:

		using Dst = Kernel::capid_t;

	private:

		Dst _dst;

		void _inc() const;
		void _dec() const;

	public:

		struct Raw
		{
			Dst dst;

			/* obsolete in base-hw, but still used in generic code path */
			addr_t local_name;
		};

		/**
		 * Create an invalid capability
		 */
		Native_capability() : _dst(Kernel::cap_id_invalid()) { }

		/**
		 * Create a capability out of a kernel's capability id
		 */
		Native_capability(Kernel::capid_t dst) : _dst(dst) { _inc(); }

		/**
		 * Create a capability from another one
		 */
		Native_capability(const Native_capability &o) : _dst(o._dst) { _inc(); }

		~Native_capability() { _dec(); }

		/**
		 * Returns true if it is a valid capability otherwise false
		 */
		bool valid() const { return (_dst != Kernel::cap_id_invalid()); }


		/*****************
		 **  Accessors  **
		 *****************/

		addr_t local_name() const { return _dst; }
		Dst    dst()        const { return _dst; }


		/**************************
		 **  Operator overloads  **
		 **************************/

		bool operator==(const Native_capability &o) const {
			return _dst == _dst; }

		Native_capability& operator=(const Native_capability &o)
		{
			if (this == &o) return *this;
			_dec();
			_dst = o._dst;
			_inc();
			return *this;
		}
};

#endif /* _INCLUDE__BASE__NATIVE_CAPABILITY_H_ */

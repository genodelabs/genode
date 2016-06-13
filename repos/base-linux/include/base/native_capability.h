/*
 * \brief  Native capability type
 * \author Norman Feske
 * \date   2007-10-15
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_CAPABILITY_H_
#define _INCLUDE__BASE__NATIVE_CAPABILITY_H_

#include <util/string.h>
#include <base/native_capability_tpl.h>
#include <base/stdint.h>

namespace Genode {

	struct Cap_dst_policy
	{
		struct Dst
		{
			int socket;

			/**
			 * Default constructor creates invalid destination
			 */
			Dst() : socket(-1) { }

			explicit Dst(int socket) : socket(socket) { }
		};

		static bool valid(Dst id) { return id.socket != -1; }
		static Dst  invalid()     { return Dst(); }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
}

#endif /* _INCLUDE__BASE__NATIVE_CAPABILITY_H_ */

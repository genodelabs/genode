/*
 * \brief  Dummy definitions for native types used for compiling unit tests
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/native_capability.h>

namespace Genode {

	struct Empty_thread_id {
		static bool valid(Empty_thread_id id) { return true; }
		static Empty_thread_id invalid()      { return Empty_thread_id();}
	};

	typedef volatile int  Native_lock;
	typedef int           Native_thread;
	typedef Native_thread Native_thread_id;
	typedef struct { }    Native_utcb;
	typedef int Native_connection_state;
	typedef Native_capability_tpl<Empty_thread_id,Empty_thread_id> Native_capability;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */

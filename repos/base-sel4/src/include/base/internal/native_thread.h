/*
 * \brief  Kernel-specific thread meta data
 * \author Norman Feske
 * \date   2016-03-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_

#include <util/noncopyable.h>
#include <base/stdint.h>

namespace Genode { struct Native_thread; }


struct Genode::Native_thread : Noncopyable
{
	struct Attr { unsigned tcb_sel, ep_sel, rcv_sel, lock_sel; } attr { };

	Native_thread() { }
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_ */

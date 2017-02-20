/*
 * \brief  Kernel-specific thread meta data
 * \author Norman Feske
 * \date   2016-03-11
 *
 * On most platforms, the 'Genode::Native_thread' type is private to the base
 * framework. However, on Fiasco.OC, we make the type publicly available to
 * expose the low-level thread-specific capability selectors to L4Linux.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC__NATIVE_THREAD_H_
#define _INCLUDE__FOC__NATIVE_THREAD_H_

/* Genode includes */
#include <base/stdint.h>
#include <foc/receive_window.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode { struct Native_thread; }

struct Genode::Native_thread
{
	Fiasco::l4_cap_idx_t kcap = 0;

	/* receive window for capability selectors received at the server side */
	Receive_window rcv_window;

	Native_thread() { }
	explicit Native_thread(Fiasco::l4_cap_idx_t kcap) : kcap(kcap) { }
};

#endif /* _INCLUDE__FOC__NATIVE_THREAD_H_ */

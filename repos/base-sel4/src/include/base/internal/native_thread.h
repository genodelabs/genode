/*
 * \brief  Kernel-specific thread meta data
 * \author Norman Feske
 * \date   2016-03-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_

#include <base/stdint.h>

namespace Genode { struct Native_thread; }

struct Genode::Native_thread
{
	unsigned tcb_sel = 0;
	unsigned ep_sel  = 0;
	unsigned rcv_sel = 0;
	unsigned lock_sel = 0;
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_ */

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

/* Genode includes */
#include <base/stdint.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/types.h>
} }

namespace Genode {

	struct Native_thread;
	class  Platform_thread;
}


struct Genode::Native_thread
{
	Okl4::L4_ThreadId_t l4id;

	/**
	 * Only used in core
	 *
	 * For 'Thread' objects created within core, 'pt' points to
	 * the physical thread object, which is going to be destroyed
	 * on destruction of the 'Thread'.
	 */
	Platform_thread *pt;
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_ */

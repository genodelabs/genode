/*
 * \brief  Fiasco-specific thread helper functions
 * \author Norman Feske
 * \date   2007-05-03
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FIASCO__THREAD_HELPER_H_
#define _INCLUDE__FIASCO__THREAD_HELPER_H_

namespace Fiasco {
#include <l4/sys/types.h>

	/**
	 * Sigma0 thread ID
	 *
	 * We must use a raw hex value initializer since we're using C++ and
	 * l4_threadid_t is an union.
	 */
	const l4_threadid_t sigma0_threadid = { 0x00040000 };
}

#endif /* _INCLUDE__FIASCO__THREAD_HELPER_H_ */

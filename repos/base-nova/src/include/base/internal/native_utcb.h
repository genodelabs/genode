/*
 * \brief  UTCB definition
 * \author Norman Feske
 * \date   2016-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_

#include <base/stdint.h>

namespace Genode { struct Native_utcb; }

class Genode::Native_utcb
{
	private:

		/**
		 * Size of the NOVA-specific user-level thread-control
		 * block
		 */
		enum { UTCB_SIZE = 4096 };

		/**
		 * User-level thread control block
		 *
		 * The UTCB is one 4K page, shared between the kernel
		 * and the user process. It is not backed by a
		 * dataspace but provided by the kernel.
		 */
		addr_t _utcb[UTCB_SIZE/sizeof(addr_t)];
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_ */


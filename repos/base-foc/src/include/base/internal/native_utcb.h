/*
 * \brief  UTCB definition
 * \author Norman Feske
 * \date   2016-03-08
 *
 * The 'Native_utcb' is located within the stack slot of the thread. We merely
 * use it for remembering a pointer to the real UTCB, which resides somewhere
 * in the kernel's address space.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_

namespace Fiasco {
#include <l4/sys/utcb.h>

	enum Utcb_regs {
		UTCB_TCR_BADGE      = 1,
		UTCB_TCR_THREAD_OBJ = 2
	};
}

namespace Genode { struct Native_utcb; }

struct Genode::Native_utcb
{
	Fiasco::l4_utcb_t *foc_utcb = nullptr;
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_ */


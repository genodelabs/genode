/*
 * \brief  Lay back and relax
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-19
 */

/*
 * Copyright (C) 2006-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SLEEP_H_
#define _INCLUDE__BASE__SLEEP_H_

/* L4/Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
}

namespace Genode {

	__attribute__((noreturn)) inline void sleep_forever()
	{
		while (true) Fiasco::l4_ipc_sleep((Fiasco::l4_timeout_t){0});
	}
}

#endif /* _INCLUDE__BASE__SLEEP_H_ */

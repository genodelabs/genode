/*
 * \brief  Connection state
 * \author Norman Feske
 * \date   2016-03-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_CONNECTION_STATE_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_CONNECTION_STATE_H_

/* Genode includes */
#include <base/stdint.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode { struct Native_connection_state; }


struct Genode::Native_connection_state
{
	Fiasco::l4_threadid_t caller;
};


#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_CONNECTION_STATE_H_ */

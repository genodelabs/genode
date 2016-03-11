/*
 * \brief  Connection state of incoming RPC request
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

#include <base/stdint.h>

namespace Genode { struct Native_connection_state; }


struct Genode::Native_connection_state
{
	int server_sd = -1;
	int client_sd = -1;
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_CONNECTION_STATE_H_ */

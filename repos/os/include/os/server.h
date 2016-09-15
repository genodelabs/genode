/*
 * \brief  Skeleton for implementing servers
 * \author Norman Feske
 * \date   2013-09-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__SERVER_H_
#define _INCLUDE__OS__SERVER_H_

#include <base/entrypoint.h>
#include <os/signal_rpc_dispatcher.h>

namespace Server {

	using namespace Genode;

	void wait_and_dispatch_one_signal();


	/***********************************************************
	 ** Functions to be provided by the server implementation **
	 ***********************************************************/

	/*
	 * Avoid the ambiguity of 'size_t' if the header is included from
	 * libc-using code.
	 */
	Genode::size_t stack_size();

	char const *name();

	void construct(Entrypoint &);
}

#endif /* _INCLUDE__OS__SERVER_H_ */

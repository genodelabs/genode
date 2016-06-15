/*
 * \brief   Platform-agnostic interface for the local capability space
 * \author  Norman Feske
 * \date    2015-05-08
 *
 * Even though the capability space is implemented in a platform-specific way,
 * all platforms have the same lifetime management in common, which uses this
 * interface.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_H_
#define _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_H_

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/capability_data.h>


namespace Genode { namespace Capability_space {

	/**
	 * Create capability for RPC entrypoint thread
	 */
	Native_capability create_ep_cap(Thread &ep_thread);

	/**
	 * Increment reference counter
	 */
	void dec_ref(Native_capability::Data &);

	/**
	 * Decrement reference counter
	 */
	void inc_ref(Native_capability::Data &);

	/**
	 * Obtain RPC object key
	 */
	Rpc_obj_key rpc_obj_key(Native_capability::Data const &);

	/**
	 * Print internal capability representation
	 */
	void print(Output &, Native_capability::Data const &);
} }

#endif /* _INCLUDE__BASE__INTERNAL__CAPABILITY_SPACE_H_ */

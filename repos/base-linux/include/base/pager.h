/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-04-28
 *
 * Linux dummies
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PAGER_H_
#define _INCLUDE__BASE__PAGER_H_

#include <base/signal.h>
#include <pager/capability.h>
#include <cap_session/cap_session.h>
#include <thread/capability.h>

namespace Genode {

	struct Pager_object
	{
		Thread_capability         _thread_cap;
		Signal_context_capability _sigh;


		virtual ~Pager_object() { }

		void exception_handler(Signal_context_capability sigh) { _sigh = sigh; }

		/**
		 * Remember thread cap so that rm_session can tell thread that
		 * rm_client is gone.
		 */
		Thread_capability thread_cap() { return _thread_cap; } const
		void thread_cap(Thread_capability cap) { _thread_cap = cap; }

		/* required by lookup_and_lock, provided by Object_pool::Entry normally */
		void release() { }
	};

	class Pager_activation_base { };
	struct Pager_entrypoint
	{
		Pager_entrypoint(Cap_session *, Pager_activation_base *) { }

		Pager_object *lookup_and_lock(Pager_capability) { return 0; }
	};
	template <int FOO> class Pager_activation : public Pager_activation_base { };
}

#endif /* _INCLUDE__BASE__PAGER_H_ */

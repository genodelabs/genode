/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-04-28
 *
 * Linux dummies
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PAGER_H_
#define _CORE__INCLUDE__PAGER_H_

/* Genode includes */
#include <base/signal.h>
#include <pager/capability.h>
#include <thread/capability.h>

/* core-local includes */
#include <rpc_cap_factory.h>

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
	};

	struct Pager_entrypoint
	{
		Pager_entrypoint(Rpc_cap_factory &) { }

		template <typename FUNC>
		auto apply(Pager_capability, FUNC f) -> decltype(f(nullptr)) {
			return f(nullptr); }

		Pager_capability manage(Pager_object *) { return Pager_capability(); }

		void dissolve(Pager_object *) { }
	};
}

#endif /* _CORE__INCLUDE__PAGER_H_ */

/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-04-28
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
#include <cpu_session/cpu_session.h>  /* for 'Thread_capability' type */
#include <pager/capability.h>

/* core includes */
#include <rpc_cap_factory.h>

namespace Core {

	struct Pager_object;
	struct Pager_entrypoint;

	using Pager_capability = Capability<Pager_object>;

	extern void init_page_fault_handling(Rpc_entrypoint &);
}


struct Core::Pager_object
{
	Thread_capability         _thread_cap { };
	Signal_context_capability _sigh       { };

	virtual ~Pager_object() { }

	void exception_handler(Signal_context_capability sigh) { _sigh = sigh; }

	/**
	 * Remember thread cap so that rm_session can tell thread that
	 * rm_client is gone.
	 */
	Thread_capability thread_cap() const { return _thread_cap; }
	void thread_cap(Thread_capability cap) { _thread_cap = cap; }
};


struct Core::Pager_entrypoint
{
	Pager_entrypoint(Rpc_cap_factory &) { }

	auto apply(Pager_capability, auto const &fn) -> decltype(fn(nullptr)) {
		return fn(nullptr); }

	Pager_capability manage(Pager_object &) { return Pager_capability(); }

	void dissolve(Pager_object &) { }
};

#endif /* _CORE__INCLUDE__PAGER_H_ */

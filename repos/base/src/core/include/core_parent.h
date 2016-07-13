/*
 * \brief  Core-specific parent client implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-20
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_PARENT_H_
#define _CORE__INCLUDE__CORE_PARENT_H_

#include <parent/parent.h>

namespace Genode { struct Core_parent; }


/**
 * In fact, Core has _no_ parent. But most of our libraries could work
 * seamlessly inside Core too, if it had one. Core_parent fills this gap.
 */
struct Genode::Core_parent : Parent
{
	void exit(int);

	void announce(Service_name const &, Root_capability) { }

	Session_capability session(Service_name const &, Session_args const &,
	                           Affinity const &);

	void upgrade(Session_capability, Upgrade_args const &) { throw Quota_exceeded(); }

	void close(Session_capability) { }

	Thread_capability main_thread_cap() const { return Thread_capability(); }

	void resource_avail_sigh(Signal_context_capability) { }

	void resource_request(Resource_args const &) { }

	void yield_sigh(Signal_context_capability) { }

	Resource_args yield_request() { return Resource_args(); }

	void yield_response() { }
};

#endif /* _CORE__INCLUDE__CORE_PARENT_H_ */

/*
 * \brief  Event tracing infrastructure
 * \author Norman Feske
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__TRACE__POLICY_H_
#define _INCLUDE__BASE__TRACE__POLICY_H_

#include <base/stdint.h>

namespace Genode {

	class Msgbuf_base;
	class Signal_context;
	class Rpc_object_base;

	namespace Trace { class Policy_module; }
}


/**
 * Header of tracing policy
 */
struct Genode::Trace::Policy_module
{
	size_t (*max_event_size)  ();
	size_t (*rpc_call)        (char *, char const *, Msgbuf_base const &);
	size_t (*rpc_returned)    (char *, char const *, Msgbuf_base const &);
	size_t (*rpc_dispatch)    (char *, char const *);
	size_t (*rpc_reply)       (char *, char const *);
	size_t (*signal_submit)   (char *, unsigned const);
	size_t (*signal_received) (char *, Signal_context const &, unsigned const);
};

#endif /* _INCLUDE__BASE__TRACE__POLICY_H_ */

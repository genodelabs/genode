/*
 * \brief  Trace-event definitions
 * \author Norman Feske
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__TRACE__EVENTS_H_
#define _INCLUDE__BASE__TRACE__EVENTS_H_

#include <base/thread.h>
#include <base/trace/policy.h>

namespace Genode { namespace Trace {

	struct Rpc_call;
	struct Rpc_returned;
	struct Rpc_dispatch;
	struct Rpc_reply;
	struct Signal_submit;
	struct Signal_received;
} }


struct Genode::Trace::Rpc_call
{
	char        const *rpc_name;
	Msgbuf_base const &msg;

	Rpc_call(char const *rpc_name, Msgbuf_base const &msg)
	: rpc_name(rpc_name), msg(msg)
	{
		Thread::trace(this);
	}

	size_t generate(Policy_module &policy, char *dst) const {
		return policy.rpc_call(dst, rpc_name, msg); }
};


struct Genode::Trace::Rpc_returned
{
	char        const *rpc_name;
	Msgbuf_base const &msg;

	Rpc_returned(char const *rpc_name, Msgbuf_base const &msg)
	: rpc_name(rpc_name), msg(msg)
	{
		Thread::trace(this);
	}

	size_t generate(Policy_module &policy, char *dst) const {
		return policy.rpc_returned(dst, rpc_name, msg); }
};


struct Genode::Trace::Rpc_dispatch
{
	char const *rpc_name;

	Rpc_dispatch(char const *rpc_name)
	:
		rpc_name(rpc_name)
	{
		Thread::trace(this);
	}

	size_t generate(Policy_module &policy, char *dst) const {
		return policy.rpc_dispatch(dst, rpc_name); }
};


struct Genode::Trace::Rpc_reply
{
	char const *rpc_name;

	Rpc_reply(char const *rpc_name)
	:
		rpc_name(rpc_name)
	{
		Thread::trace(this);
	}

	size_t generate(Policy_module &policy, char *dst) const {
		return policy.rpc_reply(dst, rpc_name); }
};


struct Genode::Trace::Signal_submit
{
	unsigned const num;

	Signal_submit(unsigned const num) : num(num)
	{ Thread::trace(this); }

	size_t generate(Policy_module &policy, char *dst) const {
		return policy.signal_submit(dst, num); }
};


struct Genode::Trace::Signal_received
{
	Signal_context const &signal_context;
	unsigned const num;

	Signal_received(Signal_context const &signal_context, unsigned num)
	:
		signal_context(signal_context), num(num)
	{
		Thread::trace(this);
	}

	size_t generate(Policy_module &policy, char *dst) const {
		return policy.signal_received(dst, signal_context, num); }
};


#endif /* _INCLUDE__BASE__TRACE__EVENTS_H_ */

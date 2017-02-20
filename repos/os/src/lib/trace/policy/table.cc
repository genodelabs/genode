/*
 * \brief  Header of tracing policy module
 * \author Norman Feske
 * \date   2013-08-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <trace/policy.h>
#include <base/trace/policy.h>

extern "C" {

	Genode::Trace::Policy_module policy_jump_table =
	{
		max_event_size,
		rpc_call,
		rpc_returned,
		rpc_dispatch,
		rpc_reply,
		signal_submit,
		signal_receive
	};
}

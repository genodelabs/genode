/*
 * \brief  Monitored CPU session
 * \author Norman Feske
 * \date   2023-05-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITORED_CPU_H_
#define _MONITORED_CPU_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <cpu_session/connection.h>

/* local includes */
#include <inferior_pd.h>
#include <monitored_native_cpu.h>

namespace Monitor { struct Monitored_cpu_session; }


struct Monitor::Monitored_cpu_session : Monitored_rpc_object<Cpu_session>
{
	using Monitored_rpc_object::Monitored_rpc_object;

	void _with_cpu_arg(Capability<Cpu_session> cpu_cap,
	                   auto const &monitored_fn, auto const &direct_fn)
	{
		if (cpu_cap == cap()) {
			error("attempt to pass invoked capability as RPC argument");
			return;
		}
		with_monitored<Monitored_cpu_session>(_ep, cpu_cap, monitored_fn, direct_fn);
	}


	/***************************
	 ** Cpu_session interface **
	 ***************************/

	int ref_account(Cpu_session_capability cpu_cap) override
	{
		int result = 0;

		_with_cpu_arg(cpu_cap,
			[&] (Monitored_cpu_session &monitored_cpu) {
				result = _real.call<Rpc_ref_account>(monitored_cpu._real); },
			[&] {
				result = _real.call<Rpc_ref_account>(cpu_cap); });

		return result;
	}

	int transfer_quota(Cpu_session_capability cpu_cap, size_t amount) override
	{
		int result = 0;

		_with_cpu_arg(cpu_cap,
			[&] (Monitored_cpu_session &monitored_cpu) {
				result = _real.call<Rpc_transfer_quota>(monitored_cpu._real, amount); },
			[&] {
				result = _real.call<Rpc_transfer_quota>(cpu_cap, amount); });

		return result;
	}
};

#endif /* _MONITORED_CPU_H_ */

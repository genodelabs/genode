/*
 * \brief  Monitored PD session
 * \author Norman Feske
 * \date   2023-05-08
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITORED_PD_H_
#define _MONITORED_PD_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <pd_session/connection.h>

/* local includes */
#include <monitored_region_map.h>
#include <monitored_thread.h>

namespace Monitor { struct Monitored_pd_session; }


struct Monitor::Monitored_pd_session : Monitored_rpc_object<Pd_session>
{
	using Monitored_rpc_object::Monitored_rpc_object;

	void _with_pd_arg(Capability<Pd_session> pd_cap,
	                  auto const &monitored_fn, auto const &direct_fn)
	{
		if (pd_cap == cap()) {
			error("attempt to pass invoked capability as RPC argument");
			return;
		}
		with_monitored<Monitored_pd_session>(_ep, pd_cap, monitored_fn, direct_fn);
	}


	/**************************
	 ** Pd_session interface **
	 **************************/

	void ref_account(Capability<Pd_session> pd_cap) override
	{
		_with_pd_arg(pd_cap,
			[&] (Monitored_pd_session &pd) { _real.call<Rpc_ref_account>(pd._real); },
			[&]                            { _real.call<Rpc_ref_account>(pd_cap); });
	}

	void transfer_quota(Capability<Pd_session> pd_cap, Cap_quota amount) override
	{
		_with_pd_arg(pd_cap,
			[&] (Monitored_pd_session &pd) {
				_real.call<Rpc_transfer_cap_quota>(pd._real, amount); },
			[&] {
				_real.call<Rpc_transfer_cap_quota>(pd_cap, amount); });
	}

	void transfer_quota(Pd_session_capability pd_cap, Ram_quota amount) override
	{
		_with_pd_arg(pd_cap,
			[&] (Monitored_pd_session &pd) {
				_real.call<Rpc_transfer_ram_quota>(pd._real, amount); },
			[&] {
				_real.call<Rpc_transfer_ram_quota>(pd_cap, amount); });
	}
};

#endif /* _MONITORED_PD_H_ */

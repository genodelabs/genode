/*
 * \brief  Device PD handling for the platform driver
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#pragma once

#include <os/static_parent_services.h>
#include <os/slave.h>

enum { STACK_SIZE = 4 * sizeof(void *) * 1024 };

namespace Platform { class Device_pd_policy; }

class Platform::Device_pd_policy
:
	private Genode::Static_parent_services<Genode::Pd_session,
	                                       Genode::Cpu_session,
	                                       Genode::Log_session,
	                                       Genode::Rom_session>,
	public Genode::Slave::Policy
{
	public:

		Device_pd_policy(Genode::Rpc_entrypoint        &slave_ep,
		                 Genode::Region_map            &local_rm,
		                 Genode::Pd_session            &pd_ref,
		                 Genode::Pd_session_capability  pd_ref_cap,
		                 Genode::Cap_quota              cap_quota,
		                 Genode::Ram_quota              ram_quota,
		                 Genode::Session_label   const &label)
		:
			Genode::Slave::Policy(label, "device_pd", *this, slave_ep, local_rm,
			                      pd_ref, pd_ref_cap, cap_quota, ram_quota)
		{ }
};


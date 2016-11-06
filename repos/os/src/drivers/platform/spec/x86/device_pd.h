/*
 * \brief  Device PD handling for the platform driver
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#pragma once

#include <os/slave.h>

enum { STACK_SIZE = 4 * sizeof(void *) * 1024 };

namespace Platform { class Device_pd_policy; }

class Platform::Device_pd_policy : public Genode::Slave::Policy
{
	protected:

		char const **_permitted_services() const override
		{
			static char const *permitted_services[] = {
				"RAM", "PD", "CPU", "LOG", "ROM", 0 };
			return permitted_services;
		};

	public:

		Device_pd_policy(Genode::Rpc_entrypoint        &slave_ep,
		                 Genode::Region_map            &local_rm,
		                 Genode::Ram_session_capability ram_ref_cap,
		                 Genode::size_t                 ram_quota,
		                 Genode::Session_label   const &label)
		:
			Genode::Slave::Policy(label, "device_pd", slave_ep, local_rm,
			                      ram_ref_cap, ram_quota)
		{ }
};



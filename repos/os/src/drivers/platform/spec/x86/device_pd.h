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

/* base */
#include <base/env.h>
#include <base/quota_guard.h>
#include <region_map/client.h>
#include <pd_session/connection.h>

/* os */
#include <io_mem_session/connection.h>

namespace Platform { class Device_pd; }

class Platform::Device_pd
{
	private:

		Genode::Pd_connection _pd;
		Genode::Session_label const &_label;

		/**
		 * Custom handling of PD-session depletion during attach operations
		 *
		 * The default implementation of 'env.rm()' automatically issues a resource
		 * request if the PD session quota gets exhausted. For the device PD, we don't
		 * want to issue resource requests but let the platform driver reflect this
		 * condition to its client.
		 */
		struct Expanding_region_map_client : Genode::Region_map_client
		{
			Genode::Env             &_env;
			Genode::Pd_connection   &_pd;
			Genode::Ram_quota_guard &_ram_guard;
			Genode::Cap_quota_guard &_cap_guard;

			Expanding_region_map_client(Genode::Env &env,
			                            Genode::Pd_connection &pd,
			                            Genode::Ram_quota_guard &ram_guard,
			                            Genode::Cap_quota_guard &cap_guard)
			:
				Region_map_client(pd.address_space()), _env(env), _pd(pd),
				_ram_guard(ram_guard), _cap_guard(cap_guard)
			{ }

			Local_addr attach(Genode::Dataspace_capability ds,
			                  Genode::size_t size = 0, Genode::off_t offset = 0,
			                  bool use_local_addr = false,
			                  Local_addr local_addr = (void *)0,
			                  bool executable = false,
			                  bool writeable  = true) override
			{
				return Genode::retry<Genode::Out_of_ram>(
					[&] () {
						return Genode::retry<Genode::Out_of_caps>(
							[&] () {
								return Region_map_client::attach(ds, size, offset,
								                                 use_local_addr,
								                                 local_addr,
								                                 executable,
								                                 writeable); },
							[&] () {
								enum { UPGRADE_CAP_QUOTA = 2 };
								Genode::Cap_quota const caps { UPGRADE_CAP_QUOTA };
								_cap_guard.withdraw(caps);
								_env.pd().transfer_quota(_pd.rpc_cap(), caps);
							}
						);
					},
					[&] () {
						enum { UPGRADE_RAM_QUOTA = 4096 };
						Genode::Ram_quota const ram { UPGRADE_RAM_QUOTA };
						_ram_guard.withdraw(ram);
						_env.pd().transfer_quota(_pd.rpc_cap(), ram);
					}
				);
			}

			Local_addr attach_at(Genode::Dataspace_capability ds,
			                     Genode::addr_t local_addr,
			                     Genode::size_t size = 0,
			                     Genode::off_t offset = 0) {
				return attach(ds, size, offset, true, local_addr); };

		} _address_space;

	public:

		Device_pd(Genode::Env &env,
		          Genode::Session_label const &label,
		          Genode::Ram_quota_guard &ram_guard,
		          Genode::Cap_quota_guard &cap_guard)
		:
			_pd(env, label.string(), Genode::Pd_connection::Virt_space::UNCONSTRAIN),
			_label(label),
			_address_space(env, _pd, ram_guard, cap_guard)
		{
			_pd.ref_account(env.pd_session_cap());
		}

		void attach_dma_mem(Genode::Dataspace_capability);
		void assign_pci(Genode::Io_mem_dataspace_capability const,
		                Genode::addr_t const, Genode::uint16_t const);
};

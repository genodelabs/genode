/*
 * \brief  Device PD handling for the platform driver
 * \author Alexander Boettcher
 * \date   2015-11-05
 */

/*
 * Copyright (C) 2015-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DEVICE_PD_H_
#define _DEVICE_PD_H_

/* base */
#include <base/env.h>
#include <base/quota_guard.h>
#include <region_map/client.h>
#include <pd_session/connection.h>
#include <io_mem_session/connection.h>

/* os */
#include <legacy/x86/platform_session/platform_session.h>

namespace Platform { class Device_pd; }


class Platform::Device_pd
{
	private:

		Pd_connection _pd;
		Session_label const &_label;

		/**
		 * Custom handling of PD-session depletion during attach operations
		 *
		 * The default implementation of 'env.rm()' automatically issues a resource
		 * request if the PD session quota gets exhausted. For the device PD, we don't
		 * want to issue resource requests but let the platform driver reflect this
		 * condition to its client.
		 */
		struct Expanding_region_map_client : Region_map_client
		{
			Env             &_env;
			Pd_connection   &_pd;
			Ram_quota_guard &_ram_guard;
			Cap_quota_guard &_cap_guard;

			Expanding_region_map_client(Env &env,
			                            Pd_connection &pd,
			                            Ram_quota_guard &ram_guard,
			                            Cap_quota_guard &cap_guard)
			:
				Region_map_client(pd.address_space()), _env(env), _pd(pd),
				_ram_guard(ram_guard), _cap_guard(cap_guard)
			{ }

			Local_addr attach(Dataspace_capability ds,
			                  size_t size = 0, off_t offset = 0,
			                  bool use_local_addr = false,
			                  Local_addr local_addr = (void *)0,
			                  bool executable = false,
			                  bool writeable  = true) override
			{
				return retry<Out_of_ram>(
					[&] () {
						return retry<Out_of_caps>(
							[&] () {
								return Region_map_client::attach(ds, size, offset,
								                                 use_local_addr,
								                                 local_addr,
								                                 executable,
								                                 writeable); },
							[&] () {
								enum { UPGRADE_CAP_QUOTA = 2 };
								Cap_quota const caps { UPGRADE_CAP_QUOTA };
								_cap_guard.withdraw(caps);
								_env.pd().transfer_quota(_pd.rpc_cap(), caps);
							}
						);
					},
					[&] () {
						enum { UPGRADE_RAM_QUOTA = 4096 };
						Ram_quota const ram { UPGRADE_RAM_QUOTA };
						_ram_guard.withdraw(ram);
						_env.pd().transfer_quota(_pd.rpc_cap(), ram);
					}
				);
			}

			Local_addr attach_at(Dataspace_capability ds,
			                     addr_t local_addr,
			                     size_t size = 0,
			                     off_t offset = 0) {
				return attach(ds, size, offset, true, local_addr); };

		} _address_space;

	public:

		Device_pd(Env &env,
		          Session_label const &label,
		          Ram_quota_guard &ram_guard,
		          Cap_quota_guard &cap_guard)
		:
			_pd(env, "device PD", Pd_connection::Virt_space::UNCONSTRAIN),
			_label(label),
			_address_space(env, _pd, ram_guard, cap_guard)
		{
			_pd.ref_account(env.pd_session_cap());
		}

		void attach_dma_mem(Dataspace_capability, addr_t dma_addr);
		void assign_pci(Io_mem_dataspace_capability const,
		                addr_t const, uint16_t const);
};

#endif /* _DEVICE_PD_H_ */

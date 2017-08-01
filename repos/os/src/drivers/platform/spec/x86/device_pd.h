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
			Genode::Env &_env;

			Expanding_region_map_client(Genode::Env &env,
			                            Genode::Capability<Genode::Region_map> address_space)
			:
				Region_map_client(address_space), _env(env)
			{ }

			Local_addr attach(Genode::Dataspace_capability ds,
			                  Genode::size_t size = 0, Genode::off_t offset = 0,
			                  bool use_local_addr = false,
			                  Local_addr local_addr = (void *)0,
			                  bool executable = false) override
			{
				return Genode::retry<Genode::Out_of_ram>(
					[&] () {
						return Genode::retry<Genode::Out_of_caps>(
							[&] () {
								return Region_map_client::attach(ds, size, offset,
								                                 use_local_addr,
								                                 local_addr,
								                                 executable); },
							[&] () {
								enum { UPGRADE_CAP_QUOTA = 2 };
								if (_env.pd().avail_caps().value < UPGRADE_CAP_QUOTA)
									throw;

								Genode::String<32> arg("cap_quota=", (unsigned)UPGRADE_CAP_QUOTA);
								_env.upgrade(Genode::Parent::Env::pd(), arg.string());
							}
						);
					},
					[&] () {
						enum { UPGRADE_RAM_QUOTA = 4096 };
						if (_env.ram().avail_ram().value < UPGRADE_RAM_QUOTA)
							throw;

						Genode::String<32> arg("ram_quota=", (unsigned)UPGRADE_RAM_QUOTA);
						_env.upgrade(Genode::Parent::Env::pd(), arg.string());
					}
				);
			}
		} _address_space;

	public:

		Device_pd(Genode::Env &env,
		          Genode::Session_label const &label)
		:
			_pd(env, label.string()),
			_label(label),
			_address_space(env, _pd.address_space())
		{ }

		void attach_dma_mem(Genode::Dataspace_capability);
		void assign_pci(Genode::Io_mem_dataspace_capability, Genode::uint16_t);
};

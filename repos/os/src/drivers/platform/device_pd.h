/*
 * \brief  Device PD handling for the platform driver
 * \author Alexander Boettcher
 * \date   2015-11-05
 */

/*
 * Copyright (C) 2015-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__DEVICE_PD_H_
#define _SRC__DRIVERS__PLATFORM__DEVICE_PD_H_

/* base */
#include <base/env.h>
#include <base/quota_guard.h>
#include <region_map/client.h>
#include <pci/types.h>
#include <pd_session/connection.h>
#include <io_mem_session/capability.h>

namespace Driver {
	using namespace Genode;
	class Device_pd;
}

class Driver::Device_pd
{
	private:

		Pd_connection _pd;

		/**
		 * Custom handling of PD-session depletion during attach operations
		 *
		 * The default implementation of 'env.rm()' automatically issues a resource
		 * request if the PD session quota gets exhausted. For the device PD, we don't
		 * want to issue resource requests but let the platform driver reflect this
		 * condition to its client.
		 */
		struct Region_map_client : Genode::Region_map_client
		{
			Env             & _env;
			Pd_connection   & _pd;
			Ram_quota_guard & _ram_guard;
			Cap_quota_guard & _cap_guard;

			Region_map_client(Env             & env,
			                  Pd_connection   & pd,
			                  Ram_quota_guard & ram_guard,
			                  Cap_quota_guard & cap_guard)
			:
				Genode::Region_map_client(pd.address_space()),
				_env(env), _pd(pd),
				_ram_guard(ram_guard), _cap_guard(cap_guard)
			{ }

			Local_addr attach(Dataspace_capability ds,
			                  size_t     size           = 0,
			                  off_t      offset         = 0,
			                  bool       use_local_addr = false,
			                  Local_addr local_addr     = (void *)0,
			                  bool       executable     = false,
			                  bool       writeable      = true) override;

			void upgrade_ram();
			void upgrade_caps();
		} _address_space;

	public:

		Device_pd(Env &env,
		          Ram_quota_guard &ram_guard,
		          Cap_quota_guard &cap_guard);

		void attach_dma_mem(Dataspace_capability, addr_t dma_addr);
		void assign_pci(Io_mem_dataspace_capability const, Pci::Bdf const);
};

#endif /* _SRC__DRIVERS__PLATFORM__DEVICE_PD_H_ */

/*
 * \brief  Device PD handling for the platform driver
 * \author Alexander Boettcher
 * \author Johannes Schlatow
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
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/quota_guard.h>
#include <region_map/client.h>
#include <pci/types.h>
#include <pd_session/connection.h>
#include <io_mem_session/capability.h>

/* local inludes */
#include <io_mmu.h>

namespace Driver {
	using namespace Genode;

	class Device_pd;
	class Kernel_iommu;
}


class Driver::Device_pd : public Io_mmu::Domain
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

		Device_pd(Env                        & env,
		          Ram_quota_guard            & ram_guard,
		          Cap_quota_guard            & cap_guard,
		          Kernel_iommu               & iommu,
		          Allocator                  & md_alloc,
		          Registry<Dma_buffer> const & buffer_registry);

		void add_range(Io_mmu::Range const &,
		               addr_t const,
		               Dataspace_capability const) override;
		void remove_range(Io_mmu::Range const &) override;

		void enable_pci_device(Io_mem_dataspace_capability const,
		                       Pci::Bdf const &) override;
		void disable_pci_device(Pci::Bdf const &) override;
};


class Driver::Kernel_iommu : public Io_mmu
{
	private:

		Env & _env;

	public:

		/**
		 * Iommu interface
		 */

		Driver::Io_mmu::Domain & create_domain(
			Allocator                  & md_alloc,
			Ram_allocator              &,
			Registry<Dma_buffer> const & buffer_registry,
			Ram_quota_guard            & ram_guard,
			Cap_quota_guard            & cap_guard) override
		{
			return *new (md_alloc) Device_pd(_env,
			                                 ram_guard,
			                                 cap_guard,
			                                 *this,
			                                 md_alloc,
			                                 buffer_registry);
		}


		Kernel_iommu(Env                      & env,
		             Io_mmu_devices           & io_mmu_devices,
		             Device::Name       const & name)
		: Io_mmu(io_mmu_devices, name),
		  _env(env)
		{ };

		~Kernel_iommu() { _destroy_domains(); }
};

#endif /* _SRC__DRIVERS__PLATFORM__DEVICE_PD_H_ */

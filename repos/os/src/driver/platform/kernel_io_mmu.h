/*
 * \brief  Platform driver - handling of IOMMUs controlled by the kernel
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

#ifndef _SRC__DRIVER__PLATFORM__KERNEL_IO_MMU_H_
#define _SRC__DRIVER__PLATFORM__KERNEL_IO_MMU_H_

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

	class Kernel_io_mmu;
}


class Driver::Kernel_io_mmu : public Io_mmu
{
	private:

		Env &_env;

		class Device_pd;

	public:

		Kernel_io_mmu(Env              &env,
		             Io_mmu_devices    &io_mmu_devices,
		             Device_name const &name);
		~Kernel_io_mmu();


		/*********************
		 ** Iommu interface **
		 *********************/

		Driver::Io_mmu::Domain & create_domain(
			Allocator                  &md_alloc,
			Ram_allocator              &,
			Ram_quota_guard            &ram_guard,
			Cap_quota_guard            &cap_guard) override;

		void destroy_domain(Allocator &, Driver::Io_mmu::Domain &) override;

		void enregister(Device const &, Domain &) override;
		void deregister(Device const &, Domain &) override;
};


class Driver::Kernel_io_mmu::Device_pd : public Io_mmu::Domain
{
	private:

		friend class Kernel_io_mmu;

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
			Env             &_env;
			Pd_connection   &_pd;
			Ram_quota_guard &_ram_guard;
			Cap_quota_guard &_cap_guard;

			Region_map_client(Env             &env,
			                  Pd_connection   &pd,
			                  Ram_quota_guard &ram_guard,
			                  Cap_quota_guard &cap_guard)
			:
				Genode::Region_map_client(pd.address_space()),
				_env(env), _pd(pd),
				_ram_guard(ram_guard), _cap_guard(cap_guard)
			{ }

			Attach_result attach(Dataspace_capability ds, Attr const &attr) override;

			[[nodiscard]] bool upgrade_ram();
			[[nodiscard]] bool upgrade_caps();
		} _address_space;

	public:

		Device_pd(Env                        &env,
		          Ram_quota_guard            &ram_guard,
		          Cap_quota_guard            &cap_guard,
		          Allocator                  &md_alloc);

		void add_range(Io_mmu::Range const &,
		               addr_t const,
		               Dataspace_capability const) override;
		void remove_range(Io_mmu::Range const &) override;
};

#endif /* _SRC__DRIVER__PLATFORM__KERNEL_IO_MMU_H_ */

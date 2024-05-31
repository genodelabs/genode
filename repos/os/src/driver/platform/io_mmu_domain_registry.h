/*
 * \brief  Platform driver - IO MMU domain wrapper and registry
 * \author Johannes Schlatow
 * \date   2023-03-27
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__IO_MMU_DOMAIN_REGISTRY_H_
#define _SRC__DRIVERS__PLATFORM__IO_MMU_DOMAIN_REGISTRY_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/registry.h>
#include <base/quota_guard.h>

/* local includes */
#include <io_mmu.h>

namespace Driver
{
	using namespace Genode;

	class Io_mmu_domain_wrapper;
	class Io_mmu_domain;
	class Io_mmu_domain_registry;
}


struct Driver::Io_mmu_domain_wrapper
{
	Io_mmu::Domain & domain;

	Io_mmu_domain_wrapper(Io_mmu                                & io_mmu,
	                      Allocator                             & md_alloc,
	                      Ram_allocator                         & ram_alloc,
	                      Registry<Dma_buffer>            const & dma_buffers,
	                      Ram_quota_guard                       & ram_guard,
	                      Cap_quota_guard                       & cap_guard)
	: domain(io_mmu.create_domain(md_alloc, ram_alloc, dma_buffers, ram_guard, cap_guard))
	{ }

	~Io_mmu_domain_wrapper() { destroy(domain.md_alloc(), &domain); }
};


struct Driver::Io_mmu_domain : private Registry<Io_mmu_domain>::Element,
                               public  Io_mmu_domain_wrapper
{
	Io_mmu_domain(Registry<Io_mmu_domain>       & registry,
	              Io_mmu                        & io_mmu,
	              Allocator                     & md_alloc,
	              Ram_allocator                 & ram_alloc,
	              Registry<Dma_buffer>    const & dma_buffers,
	              Ram_quota_guard               & ram_guard,
	              Cap_quota_guard               & cap_guard)
	: Registry<Io_mmu_domain>::Element(registry, *this),
	  Io_mmu_domain_wrapper(io_mmu, md_alloc, ram_alloc, dma_buffers, ram_guard, cap_guard)
	{ }
};


class Driver::Io_mmu_domain_registry : public Registry<Io_mmu_domain>
{
	protected:

		Constructible<Io_mmu_domain_wrapper> _default_domain { };

	public:

		void default_domain(Io_mmu                     & io_mmu,
		                    Allocator                  & md_alloc,
		                    Ram_allocator              & ram_alloc,
		                    Registry<Dma_buffer> const & dma_buffers,
		                    Ram_quota_guard            & ram_quota_guard,
		                    Cap_quota_guard            & cap_quota_guard)
		{
			_default_domain.construct(io_mmu, md_alloc, ram_alloc, dma_buffers,
			                          ram_quota_guard, cap_quota_guard);
		}

		template <typename FN>
		void for_each_domain(FN && fn)
		{
			for_each([&] (Io_mmu_domain & wrapper) {
				fn(wrapper.domain); });

			if (_default_domain.constructed())
				fn(_default_domain->domain);
		}

		template <typename MATCH_FN, typename NONMATCH_FN>
		void with_domain(Device::Name const &  name,
		                 MATCH_FN           && match_fn,
		                 NONMATCH_FN        && nonmatch_fn)
		{
			bool exists = false;

			for_each_domain([&] (Io_mmu::Domain & domain) {
				if (domain.device_name() == name) {
					match_fn(domain);
					exists = true;
				}
			});

			if (!exists)
				nonmatch_fn();
		}

		template <typename FN>
		void with_default_domain(FN && fn)
		{
			if (_default_domain.constructed())
				fn(_default_domain->domain);
		}
};


#endif /* _SRC__DRIVERS__PLATFORM__IO_MMU_DOMAIN_REGISTRY_H_ */

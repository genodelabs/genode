/*
 * \brief  PD root interface
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PD_ROOT_H_
#define _CORE__INCLUDE__PD_ROOT_H_

/* Genode */
#include <root/component.h>

/* Core */
#include <pd_session_component.h>

namespace Core { class Pd_root; }


class Core::Pd_root : public Root_component<Pd_session_component>
{
	private:

		Rpc_entrypoint   &_ep;
		Rpc_entrypoint   &_signal_ep;
		Pager_entrypoint &_pager_ep;
		Range_allocator  &_phys_alloc;
		Region_map       &_local_rm;
		Range_allocator  &_core_mem;
		System_control   &_system_control;

		/**
		 * The RAM allocations of system management components are getting
		 * constrained to support older devices with 32-bit physical memory
		 * address capabilities only, and to by compliant to certain 32-bit
		 * kernel limitations mapping device memory 1:1 into the lower 3 GB
		 */
		static Ram_dataspace_factory::Phys_range _phys_range_from_args(char const *args)
		{
			if (_managing_system(args) == Pd_session_component::Managing_system::DENIED)
				return Ram_dataspace_factory::any_phys_range();

			/*
			 * Leave out first page because currently return value zero
			 * for dma_addr is recognized as a fault
			 */
			addr_t const start = 0x1000;
			addr_t const end   = (sizeof(long) == 4) /* 32bit arch ? */
			                   ? 0xbfffffffUL : 0xffffffffUL;

			return Ram_dataspace_factory::Phys_range { start, end };
		}

		static Ram_dataspace_factory::Virt_range _virt_range_from_args(char const *args)
		{
			addr_t const constrained = Arg_string::find_arg(args, "virt_space")
			                           .ulong_value(Pd_connection::Virt_space::CONSTRAIN);

			if (!constrained)
				return Ram_dataspace_factory::Virt_range { 0x1000, 0UL - 0x2000 };

			return Ram_dataspace_factory::Virt_range { platform().vm_start(),
			                                           platform().vm_size() };
		}

		static Pd_session_component::Managing_system _managing_system(char const * args)
		{
			return (Arg_string::find_arg(args,
			                             "managing_system").bool_value(false))
				? Pd_session_component::Managing_system::PERMITTED
				: Pd_session_component::Managing_system::DENIED;
		}

	protected:

		Pd_session_component *_create_session(const char *args) override
		{
			return new (md_alloc())
				Pd_session_component(_ep,
				                     _signal_ep,
				                     session_resources_from_args(args),
				                     session_label_from_args(args),
				                     session_diag_from_args(args),
				                     _phys_alloc,
				                     _phys_range_from_args(args),
				                     _virt_range_from_args(args),
				                     _managing_system(args),
				                     _local_rm, _pager_ep, args,
				                     _core_mem, _system_control);
		}

		void _upgrade_session(Pd_session_component *pd, const char *args) override
		{
			pd->upgrade(ram_quota_from_args(args));
			pd->upgrade(cap_quota_from_args(args));
		}

	public:

		/**
		 * Constructor
		 */
		Pd_root(Rpc_entrypoint   &ep,
		        Rpc_entrypoint   &signal_ep,
		        Pager_entrypoint &pager_ep,
		        Range_allocator  &phys_alloc,
		        Region_map       &local_rm,
		        Allocator        &md_alloc,
		        Range_allocator  &core_mem,
		        System_control   &system_control)
		:
			Root_component<Pd_session_component>(&ep, &md_alloc),
			_ep(ep), _signal_ep(signal_ep), _pager_ep(pager_ep),
			_phys_alloc(phys_alloc), _local_rm(local_rm), _core_mem(core_mem),
			_system_control(system_control)
		{ }
};

#endif /* _CORE__INCLUDE__PD_ROOT_H_ */

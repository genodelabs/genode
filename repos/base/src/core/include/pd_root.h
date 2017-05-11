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

namespace Genode {
	class Pd_root;
}


class Genode::Pd_root : public Genode::Root_component<Genode::Pd_session_component>
{
	private:

		Rpc_entrypoint   &_ep;
		Pager_entrypoint &_pager_ep;
		Range_allocator  &_phys_alloc;
		Region_map       &_local_rm;

		static Ram_dataspace_factory::Phys_range _phys_range_from_args(char const *args)
		{
			addr_t const start = Arg_string::find_arg(args, "phys_start").ulong_value(0);
			addr_t const size  = Arg_string::find_arg(args, "phys_size").ulong_value(0);
			addr_t const end   = start + size - 1;

			return (start <= end) ? Ram_dataspace_factory::Phys_range { start, end }
			                      : Ram_dataspace_factory::any_phys_range();
		}

	protected:

		Pd_session_component *_create_session(const char *args)
		{
			return new (md_alloc())
				Pd_session_component(_ep,
				                     session_resources_from_args(args),
				                     session_label_from_args(args),
				                     session_diag_from_args(args),
				                     _phys_alloc,
				                     _phys_range_from_args(args),
				                     _local_rm, _pager_ep, args);
		}

		void _upgrade_session(Pd_session_component *pd, const char *args)
		{
			pd->Ram_quota_guard::upgrade(ram_quota_from_args(args));
			pd->Cap_quota_guard::upgrade(cap_quota_from_args(args));
			pd->session_quota_upgraded();
		}

	public:

		/**
		 * Constructor
		 */
		Pd_root(Rpc_entrypoint   &ep,
		        Pager_entrypoint &pager_ep,
		        Range_allocator  &phys_alloc,
		        Region_map       &local_rm,
		        Allocator        &md_alloc)
		:
			Root_component<Pd_session_component>(&ep, &md_alloc),
			_ep(ep), _pager_ep(pager_ep), _phys_alloc(phys_alloc), _local_rm(local_rm)
		{ }
};

#endif /* _CORE__INCLUDE__PD_ROOT_H_ */

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
		Ram_allocator    &_ram_alloc;
		Region_map       &_local_rm;

	protected:

		Pd_session_component *_create_session(const char *args)
		{
			Pd_session_component *result = new (md_alloc())
				Pd_session_component(_ep,
				                     session_resources_from_args(args),
				                     session_label_from_args(args),
				                     session_diag_from_args(args),
				                     _ram_alloc, _local_rm, _pager_ep, args);
			return result;
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
		 *
		 * \param ep        entry point for managing pd session objects,
		 *                  threads, and signal contexts
		 * \param ram_alloc allocator used for session-local allocations
		 * \param md_alloc  meta-data allocator to be used by root component
		 */
		Pd_root(Rpc_entrypoint   &ep,
		        Pager_entrypoint &pager_ep,
		        Ram_allocator    &ram_alloc,
		        Region_map       &local_rm,
		        Allocator        &md_alloc)
		:
			Root_component<Pd_session_component>(&ep, &md_alloc),
			_ep(ep), _pager_ep(pager_ep), _ram_alloc(ram_alloc), _local_rm(local_rm)
		{ }
};

#endif /* _CORE__INCLUDE__PD_ROOT_H_ */

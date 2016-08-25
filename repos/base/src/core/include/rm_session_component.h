/*
 * \brief  RM session interface
 * \author Norman Feske
 * \date   2016-04-15
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__RM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__RM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator_guard.h>
#include <base/rpc_server.h>
#include <rm_session/rm_session.h>

/* core includes */
#include <region_map_component.h>

namespace Genode { class Rm_session_component; }


class Genode::Rm_session_component : public Rpc_object<Rm_session>
{
	private:

		Rpc_entrypoint   &_ep;
		Allocator_guard   _md_alloc;
		Pager_entrypoint &_pager_ep;

		Lock                       _region_maps_lock;
		List<Region_map_component> _region_maps;

		void _destroy(Region_map_component &rmc)
		{
			_region_maps.remove(&rmc);
			Genode::destroy(_md_alloc, &rmc);
		}

	public:

		/**
		 * Constructor
		 */
		Rm_session_component(Rpc_entrypoint   &ep,
		                     Allocator        &md_alloc,
		                     Pager_entrypoint &pager_ep,
		                     size_t            ram_quota)
		:
			_ep(ep), _md_alloc(&md_alloc, ram_quota), _pager_ep(pager_ep)
		{ }

		~Rm_session_component()
		{
			Lock::Guard guard(_region_maps_lock);

			while (Region_map_component *rmc = _region_maps.first())
				_destroy(*rmc);
		}

		/**
		 * Register quota donation at allocator guard
		 */
		void upgrade_ram_quota(size_t ram_quota) { _md_alloc.upgrade(ram_quota); }


		/**************************
		 ** Rm_session interface **
		 **************************/

		Capability<Region_map> create(size_t size) override
		{
			Lock::Guard guard(_region_maps_lock);

			try {
				Region_map_component *rm =
					new (_md_alloc)
						Region_map_component(_ep, _md_alloc, _pager_ep, 0, size);

				_region_maps.insert(rm);

				return rm->cap();
			}
			catch (Allocator::Out_of_memory) { throw Out_of_metadata(); }
		}

		void destroy(Capability<Region_map> rm) override
		{
			Lock::Guard guard(_region_maps_lock);

			_ep.apply(rm, [&] (Region_map_component *rmc) {
				if (!rmc) {
					warning("could not look up region map to destruct");
					return;
				}

				_destroy(*rmc);
			});
		}
};

#endif /* _CORE__INCLUDE__RM_SESSION_COMPONENT_H_ */

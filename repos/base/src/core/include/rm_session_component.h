/*
 * \brief  RM session interface
 * \author Norman Feske
 * \date   2016-04-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__RM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <rm_session/rm_session.h>
#include <base/session_object.h>

/* core includes */
#include <region_map_component.h>

namespace Genode { class Rm_session_component; }


class Genode::Rm_session_component : public Session_object<Rm_session>
{
	private:

		Rpc_entrypoint           &_ep;
		Constrained_ram_allocator _ram_alloc;
		Sliced_heap               _md_alloc;
		Pager_entrypoint         &_pager_ep;

		Mutex                      _region_maps_lock { };
		List<Region_map_component> _region_maps      { };

	public:

		/**
		 * Constructor
		 */
		Rm_session_component(Rpc_entrypoint   &ep,
		                     Resources  const &resources,
		                     Label      const &label,
		                     Diag       const &diag,
		                     Ram_allocator    &ram_alloc,
		                     Region_map       &local_rm,
		                     Pager_entrypoint &pager_ep)
		:
			Session_object(ep, resources, label, diag),
			_ep(ep),
			_ram_alloc(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
			_md_alloc(_ram_alloc, local_rm),
			_pager_ep(pager_ep)
		{ }

		~Rm_session_component()
		{
			Mutex::Guard guard(_region_maps_lock);

			while (Region_map_component *rmc = _region_maps.first()) {
				_region_maps.remove(rmc);
				Genode::destroy(_md_alloc, rmc);
			}
		}


		/**************************
		 ** Rm_session interface **
		 **************************/

		Capability<Region_map> create(size_t size) override
		{
			Mutex::Guard guard(_region_maps_lock);

			try {
				Region_map_component *rm =
					new (_md_alloc)
						Region_map_component(_ep, _md_alloc, _pager_ep, 0, size,
						                     Diag{false});

				_region_maps.insert(rm);

				return rm->cap();
			}
			catch (Allocator::Out_of_memory) { throw Out_of_ram(); }
		}

		void destroy(Capability<Region_map> cap) override
		{
			Mutex::Guard guard(_region_maps_lock);

			Region_map_component *rm = nullptr;

			_ep.apply(cap, [&] (Region_map_component *rmc) {
				if (!rmc) {
					warning("could not look up region map to destruct");
					return;
				}

				_region_maps.remove(rmc);
				rm = rmc;
			});

			if (rm)
				Genode::destroy(_md_alloc, rm);
		}
};

#endif /* _CORE__INCLUDE__RM_SESSION_COMPONENT_H_ */

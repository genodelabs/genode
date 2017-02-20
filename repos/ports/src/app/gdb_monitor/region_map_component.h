/*
 * \brief  Region map interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REGION_MAP_COMPONENT_H_
#define _REGION_MAP_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <pd_session/capability.h>
#include <region_map/client.h>

/* GDB monitor includes */
#include "dataspace_object.h"

namespace Gdb_monitor {

	using namespace Genode;

	class Region_map_component : public Rpc_object<Region_map>
	{
		private:

			Rpc_entrypoint        &_ep;

			Allocator             &_alloc;

			Pd_session_capability  _pd;

			Region_map_client      _parent_region_map;

		public:

			class Region : public Avl_node<Region>
			{
				private:
				  void *_start;
				  void *_end;
				  off_t _offset;
				  Dataspace_capability _ds_cap;

				public:
					Region(void *start, void *end, Dataspace_capability ds_cap, Genode::off_t offset)
					: _start(start), _end(end), _offset(offset), _ds_cap(ds_cap) {}

					bool higher(Region *e) { return e->_start > _start; }

					Region *find_by_addr(void *addr)
					{
						if ((addr >= _start) && (addr <= _end))
							return this;

						Region *region = child(addr > _start);
						return region ? region->find_by_addr(addr) : 0;
					}

					void *start() { return _start; }
					off_t offset() { return _offset; }
					Dataspace_capability ds_cap() { return _ds_cap; }
			};

		private:

			Avl_tree<Region>  _region_map;
			Lock              _region_map_lock;
			Dataspace_pool   &_managed_ds_map;

		public:

			/**
			 * Constructor
			 */
			Region_map_component(Rpc_entrypoint        &ep,
			                     Allocator             &alloc,
			                     Dataspace_pool        &managed_ds_map,
			                     Pd_session_capability  pd,
			                     Capability<Region_map> parent_region_map);

			~Region_map_component();

			/**
			 * Find region for given address
			 *
			 * \param  local_addr        lookup address
			 * \param  offset_in_region  translated address in looked up region
			 */
			Region *find_region(void *local_addr, addr_t *offset_in_region);


			/**************************************
			 ** Region manager session interface **
			 **************************************/

			Local_addr       attach        (Dataspace_capability, size_t,
			                                off_t, bool, Local_addr, bool) override;
			void             detach        (Local_addr) override;
			void             fault_handler (Signal_context_capability) override;
			State            state         () override;
			Dataspace_capability dataspace () override;
	};

}

#endif /* _REGION_MAP_COMPONENT_H_ */

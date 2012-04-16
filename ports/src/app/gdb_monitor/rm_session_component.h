/*
 * \brief  RM session interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _RM_SESSION_COMPONENT_H_
#define _RM_SESSION_COMPONENT_H_

/* Genode includes */
#include <rm_session/connection.h>
#include <rm_session/rm_session.h>
#include <base/rpc_server.h>

/* GDB monitor includes */
#include "dataspace_object.h"

namespace Gdb_monitor {

	using namespace Genode;

	class Rm_session_component : public Rpc_object<Rm_session>
	{
		private:

			Genode::Rm_session_client _parent_rm_session;

		public:

			class Region : public Avl_node<Region>
			{
				private:
				  void *_start;
				  void *_end;
				  Genode::off_t _offset;
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
					Genode::off_t offset() { return _offset; }
					Dataspace_capability ds_cap() { return _ds_cap; }
			};

		private:

			Avl_tree<Region>               _region_map;
			Lock                           _region_map_lock;
			Object_pool<Dataspace_object> *_managed_ds_map;

		public:

			/**
			 * Constructor
			 */
			Rm_session_component(Object_pool<Dataspace_object> *managed_ds_map,
			                     const char *args);

			~Rm_session_component();

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

			Local_addr       attach        (Dataspace_capability, Genode::size_t,
			                                Genode::off_t, bool, Local_addr, bool);
			void             detach        (Local_addr);
			Pager_capability add_client    (Thread_capability);
			void             fault_handler (Signal_context_capability handler);
			State            state         ();
			Dataspace_capability dataspace ();
	};

}

#endif /* _RM_SESSION_COMPONENT_H_ */

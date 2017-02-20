/*
 * \brief  RAM root interface
 * \author Norman Feske
 * \date   2006-05-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RAM_ROOT_H_
#define _CORE__INCLUDE__RAM_ROOT_H_

#include <root/component.h>

#include "ram_session_component.h"

namespace Genode {

	class Ram_root : public Root_component<Ram_session_component>
	{
		private:

			Range_allocator *_ram_alloc;
			Rpc_entrypoint  *_ds_ep;

		protected:

			Ram_session_component *_create_session(const char *args)
			{
				return new (md_alloc())
					Ram_session_component(_ds_ep, ep(), _ram_alloc,
					                      md_alloc(), args);
			}

			void _upgrade_session(Ram_session_component *ram, const char *args)
			{
				size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
				ram->upgrade_ram_quota(ram_quota);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  entry point for managing ram session objects
			 * \param ds_ep       entry point for managing dataspaces
			 * \param ram_alloc   pool of memory to be assigned to ram sessions
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Ram_root(Rpc_entrypoint  *session_ep,
			         Rpc_entrypoint  *ds_ep,
			         Range_allocator *ram_alloc,
			         Allocator       *md_alloc)
			:
				Root_component<Ram_session_component>(session_ep, md_alloc),
				_ram_alloc(ram_alloc), _ds_ep(ds_ep) { }
	};
}

#endif /* _CORE__INCLUDE__RAM_ROOT_H_ */

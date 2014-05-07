/*
 * \brief  ROM session implementation used by Noux processes
 * \author Norman Feske
 * \date   2013-07-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__ROM_SESSION_COMPONENT_H_
#define _NOUX__ROM_SESSION_COMPONENT_H_

/* Genode includes */
#include <rom_session/connection.h>
#include <base/rpc_server.h>

namespace Noux {

	struct Rom_dataspace_info : Dataspace_info
	{
		Rom_dataspace_info(Dataspace_capability ds) : Dataspace_info(ds) { }

		~Rom_dataspace_info() { }

		Dataspace_capability fork(Ram_session_capability,
		                          Dataspace_registry &ds_registry,
		                          Rpc_entrypoint &)
		{
			return ds_cap();
		}

		void poke(addr_t dst_offset, void const *src, size_t len)
		{
			PERR("Attempt to poke onto a ROM dataspace");
		}
	};


	class Rom_session_component : public Rpc_object<Rom_session>
	{
		private:

			/**
			 * Wrapped ROM session at core
			 */
			Rom_connection _rom;

			Dataspace_registry &_ds_registry;

			Rom_dataspace_info _ds_info;

		public:

			Rom_session_component(Dataspace_registry &ds_registry,
			                      char const *name)
			:
				_rom(name), _ds_registry(ds_registry), _ds_info(_rom.dataspace())
			{
				_ds_registry.insert(&_ds_info);
			}

			~Rom_session_component()
			{
				/*
				 * Lookup and lock ds info instead of directly accessing
				 * the '_ds_info' member.
				 */
				Object_pool<Dataspace_info>::Guard
					info(_ds_registry.lookup_info(_ds_info.ds_cap()));

				if (!info) {
					PERR("~Rom_session_component: unexpected !info");
					return;
				}

				_ds_registry.remove(&_ds_info);

				info->dissolve_users();
			}


			/***************************
			 ** ROM session interface **
			 ***************************/

			Rom_dataspace_capability dataspace()
			{
				return static_cap_cast<Rom_dataspace>(_ds_info.ds_cap());
			}

			void sigh(Signal_context_capability) { }
	};
}

#endif /* _NOUX__ROM_SESSION_COMPONENT_H_ */

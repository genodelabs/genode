/*
 * \brief  ROM service provided to Noux processes
 * \author Norman Feske
 * \date   2013-07-18
 *
 * The local ROM service has the sole purpose of tracking ROM dataspaces
 * so that they are properly detached from RM sessions when the corresponding
 * ROM sessions are closed.
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__LOCAL_ROM_SERVICE_H_
#define _NOUX__LOCAL_ROM_SERVICE_H_

/* Genode includes */
#include <base/service.h>

/* Noux includes */
#include <dataspace_registry.h>
#include <rom_session_component.h>

namespace Noux {

	typedef Local_service<Rom_session_component> Local_rom_service;
	class Local_rom_factory;
}


class Noux::Local_rom_factory : public Local_rom_service::Factory
{
	private:

		Rpc_entrypoint       &_ep;
		Vfs::Dir_file_system &_root_dir;
		Dataspace_registry   &_registry;

	public:

		Local_rom_factory(Rpc_entrypoint &ep, Vfs::Dir_file_system &root_dir,
		                  Dataspace_registry &registry)
		:
			_ep(ep), _root_dir(root_dir), _registry(registry)
		{ }

		Rom_session_component &create(Args const &args, Affinity) override
		{
			try {
				Rom_session_component::Name const rom_name =
					label_from_args(args.string()).last_element();

				return *new (env()->heap())
					Rom_session_component(_ep, _root_dir, _registry, rom_name);
			}
			catch (Rom_connection::Rom_connection_failed) { throw Denied(); }
		}

		void upgrade(Rom_session_component &, Args const &) override { }

		void destroy(Rom_session_component &session) override
		{
			Genode::destroy(env()->heap(), &session);
		}
};

#endif /* _NOUX__LOCAL_ROM_SERVICE_H_ */

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

	class Local_rom_service : public Service
	{
		private:

			Rpc_entrypoint     &_ep;
			Dataspace_registry &_ds_registry;

		public:

			Local_rom_service(Rpc_entrypoint &ep, Dataspace_registry &ds_registry)
			:
				Service(Rom_session::service_name()), _ep(ep),
				_ds_registry(ds_registry)
			{ }

			Genode::Session_capability session(const char *args, Affinity const &)
			{
				Session_label const label = label_from_args(args);

				try {
					Genode::Session_label const module_name = label.last_element();
					Rom_session_component *rom = new (env()->heap())
						Rom_session_component(_ds_registry, module_name.string());

					return _ep.manage(rom);
				} catch (Rom_connection::Rom_connection_failed) {
					throw Service::Unavailable();
				}
			}

			void upgrade(Genode::Session_capability, const char *args) { }

			void close(Genode::Session_capability session)
			{
				/* acquire locked session object */
				Rom_session_component *rom_session;

				_ep.apply(session, [&] (Rom_session_component *rsc) {
					rom_session = rsc;

					if (!rom_session) {
						PWRN("Unexpected call of close with non-ROM-session argument");
						return;
					}

					_ep.dissolve(rom_session);
				});

				destroy(env()->heap(), rom_session);
			}
	};
}

#endif /* _NOUX__LOCAL_ROM_SERVICE_H_ */

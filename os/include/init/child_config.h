/*
 * \brief  Utility for handling child configuration
 * \author Norman Feske
 * \date   2008-03-22
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INIT__CHILD_CONFIG_H_
#define _INCLUDE__INIT__CHILD_CONFIG_H_

#include <base/env.h>
#include <base/printf.h>
#include <util/xml_node.h>
#include <rom_session/connection.h>
#include <ram_session/client.h>

namespace Init {

	class Child_config
	{
		private:

			enum { CONFIGFILE_NAME_LEN = 64 };

			Genode::Ram_session_capability   _ram_session_cap;
			Genode::Rom_session_capability   _rom_session_cap;
			Genode::Rom_dataspace_capability _config_rom_ds;
			Genode::Ram_dataspace_capability _config_ram_ds;

		public:

			/**
			 * Constructor
			 *
			 * The provided RAM session is used to obtain a dataspace
			 * for holding the copy of the child's configuration data.
			 * Normally, the child's RAM session should be used to
			 * account the consumed RAM quota to the child.
			 */
			Child_config(Genode::Ram_session_capability ram_session,
			             Genode::Xml_node start_node)
			: _ram_session_cap(ram_session)
			{
				using namespace Genode;

				/*
				 * If the start node contains a 'filename' entry,
				 * we obtain the specified file from ROM.
				 */
				char filename[CONFIGFILE_NAME_LEN];
				try {
					Xml_node configfile_node = start_node.sub_node("configfile");
					configfile_node.attribute("name").value(filename, sizeof(filename));

					Rom_connection rom(filename);
					rom.on_destruction(Rom_connection::KEEP_OPEN);
					_rom_session_cap = rom.cap();
					_config_rom_ds = rom.dataspace();
				} catch (...) { }

				/*
				 * If the start node contains a 'config' entry,
				 * we copy this entry into a fresh dataspace to
				 * be provided to our child.
				 */
				Ram_session_client rsc(_ram_session_cap);
				try {
					Xml_node config_node = start_node.sub_node("config");

					const char *config = config_node.addr();
					Genode::size_t config_size = config_node.size();

					if (!config || !config_size) return;

					/*
					 * Allocate RAM dataspace that is big enough to
					 * hold the configuration and the null termination.
					 */
					_config_ram_ds = rsc.alloc(config_size + 1);

					/*
					 * Make dataspace locally accessible, copy
					 * configuration into the dataspace, and append
					 * a string-terminating zero.
					 */
					void *addr = env()->rm_session()->attach(_config_ram_ds);

					Genode::memcpy(addr, config, config_size);
					static_cast<char *>(addr)[config_size] = 0;
					env()->rm_session()->detach(addr);

				} catch (Rm_session::Attach_failed) {
					rsc.free(_config_ram_ds);
					return;
				} catch (Ram_session::Alloc_failed) {
					return;
				} catch (Xml_node::Nonexistent_sub_node) { }
			}

			/**
			 * Destructor
			 */
			~Child_config()
			{
				using namespace Genode;

				/*
				 * The configuration data is either provided as a
				 * ROM dataspace (holding a complete configfile) or
				 * as a RAM dataspace holding a copy of the start
				 * node's config entry.
				 */
				if (_rom_session_cap.valid())
					env()->parent()->close(_rom_session_cap);
				else
					Ram_session_client(_ram_session_cap).free(_config_ram_ds);
			}

			/**
			 * Request dataspace holding the start node's configuration data
			 */
			Genode::Dataspace_capability dataspace() {
				return _rom_session_cap.valid() ? Genode::Dataspace_capability(_config_rom_ds)
				                                : Genode::Dataspace_capability(_config_ram_ds); }
	};
}

#endif /* _INCLUDE__INIT__CHILD_CONFIG_H_ */

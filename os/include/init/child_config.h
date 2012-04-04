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
			char _filename[CONFIGFILE_NAME_LEN];

			Genode::Ram_session_capability   _ram_session_cap;
			Genode::Ram_dataspace_capability _config_ram_ds;

		public:

			/**
			 * Constructor
			 *
			 * The provided RAM session is used to obtain a dataspace for
			 * holding the copy of the child's configuration data unless the
			 * configuration is supplied via a config file. Normally, the
			 * child's RAM session should be used to account the consumed RAM
			 * quota to the child.
			 */
			Child_config(Genode::Ram_session_capability ram_session,
			             Genode::Xml_node               start_node)
			: _ram_session_cap(ram_session)
			{
				using namespace Genode;

				/*
				 * If the start node contains a 'filename' entry, we only keep
				 * the information about the file name.
				 */
				_filename[0] = 0;
				try {
					Xml_node configfile_node = start_node.sub_node("configfile");
					configfile_node.attribute("name")
						.value(_filename, sizeof(_filename));

					return;
				} catch (...) { }

				/*
				 * If the start node contains a 'config' entry, we copy this
				 * entry into a fresh dataspace to be provided to our child.
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
				 * The configuration data is either provided as a ROM session
				 * (holding a complete configfile) or as a RAM dataspace
				 * holding a copy of the start node's config entry. In the
				 * latter case, the child's configuration resides in a
				 * shadow copy kept in '_config_ram_ds'.
				 */
				if (_config_ram_ds.valid())
					Ram_session_client(_ram_session_cap).free(_config_ram_ds);
			}

			/**
			 * Return file name if configuration comes from a file
			 *
			 * If the configuration is provided inline, the function returns 0.
			 */
			char const *filename() const {
				return _filename[0] != 0 ? _filename : 0; }

			/**
			 * Request dataspace holding the start node's configuration data
			 *
			 * This function returns a valid dataspace only when using an
			 * inline configuration (if 'filename()' returns 0).
			 */
			Genode::Dataspace_capability dataspace() {
				return Genode::Dataspace_capability(_config_ram_ds); }
	};
}

#endif /* _INCLUDE__INIT__CHILD_CONFIG_H_ */

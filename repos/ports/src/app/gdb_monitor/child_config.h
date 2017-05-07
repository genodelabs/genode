/*
 * \brief  Utility for handling child configuration
 * \author Norman Feske
 * \date   2008-03-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILD_CONFIG_H_
#define _CHILD_CONFIG_H_

#warning header is deprecated, used os/dynamic_rom_session.h instead

#include <util/xml_node.h>
#include <base/attached_dataspace.h>
#include <ram_session/ram_session.h>

namespace Init { class Child_config; }


class Init::Child_config
{
	private:

		Genode::Ram_session &_ram;

		typedef Genode::String<64> Rom_name;
		Rom_name const _rom_name;

		Genode::Ram_dataspace_capability const _ram_ds;

		Rom_name _rom_name_from_start_node(Genode::Xml_node start)
		{
			if (!start.has_sub_node("configfile"))
				return Rom_name();

			return start.sub_node("configfile").attribute_value("name", Rom_name());
		}

		/**
		 * Buffer '<config>' sub node in a dedicated RAM dataspace
		 *
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 * \throw Region_map::Region_conflict
		 */
		Genode::Ram_dataspace_capability
		_ram_ds_from_start_node(Genode::Xml_node start,
		                        Genode::Ram_session &ram, Genode::Region_map &rm)
		{
			/*
			 * If the start node contains a 'config' entry, we copy this entry
			 * into a fresh dataspace to be provided to our child.
			 */
			Genode::Xml_node const config = start.has_sub_node("config")
			                              ? start.sub_node("config")
			                              : Genode::Xml_node("<config/>");

			Genode::Ram_dataspace_capability ram_ds;
			try {
				/*
				 * Allocate RAM dataspace that is big enough to hold the
				 * configuration and the null termination.
				 */
				ram_ds = ram.alloc(config.size() + 1);

				/*
				 * Make dataspace locally accessible, copy configuration into the
				 * dataspace, and append a string-terminating zero.
				 */
				Genode::Attached_dataspace attached(rm, ram_ds);

				Genode::memcpy(attached.local_addr<char>(),
				               config.addr(), config.size());

				attached.local_addr<char>()[config.size()] = 0;

				return ram_ds;
			}
			catch (Genode::Region_map::Region_conflict) { ram.free(ram_ds); throw; }
		}

	public:

		/**
		 * Constructor
		 *
		 * The provided RAM session is used to obtain a dataspace for
		 * holding the copy of the child's configuration data unless the
		 * configuration is supplied via a config ROM module.
		 *
		 * \throw Out_of_ram                   failed to allocate the backing
		 *                                     store for holding config data
		 * \throw Out_of_caps
		 *
		 * \throw Region_map::Region_conflict  failed to temporarily attach the
		 *                                     config dataspace to the local
		 *                                     address space
		 *
		 * If the start node contains a 'filename' entry, we only keep the
		 * information about the ROM module name.
		 */
		Child_config(Genode::Ram_session &ram, Genode::Region_map &local_rm,
		             Genode::Xml_node start)
		:
			_ram(ram),
			_rom_name(_rom_name_from_start_node(start)),
			_ram_ds(_rom_name.valid() ? Genode::Ram_dataspace_capability()
			                          : _ram_ds_from_start_node(start, ram, local_rm))
		{ }

		/**
		 * Destructor
		 */
		~Child_config() { if (_ram_ds.valid()) _ram.free(_ram_ds); }

		/**
		 * Return file name if configuration comes from a file
		 *
		 * If the configuration is provided inline, the method returns 0.
		 */
		char const *filename() const {
			return _rom_name.valid() ? _rom_name.string() : nullptr; }

		/**
		 * Request dataspace holding the start node's configuration data
		 *
		 * This method returns a valid dataspace only when using an
		 * inline configuration (if 'filename()' returns 0).
		 */
		Genode::Dataspace_capability dataspace() {
			return Genode::Dataspace_capability(_ram_ds); }
};

#endif /* _CHILD_CONFIG_H_ */

/*
 * \brief  Access to process configuration
 * \author Norman Feske
 * \date   2010-05-04
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__CONFIG_H_
#define _INCLUDE__OS__CONFIG_H_

#include <rom_session/connection.h>
#include <dataspace/client.h>
#include <util/xml_node.h>
#include <base/exception.h>

namespace Genode {

	class Config;

	/**
	 * Return singleton instance of config
	 */
	Volatile_object<Config> &config();
}


class Genode::Config
{
	private:

		Rom_connection       _config_rom;
		Dataspace_capability _config_ds;
		Xml_node             _config_xml;

	public:

		/**
		 * Constructor
		 */
		Config();

		Xml_node xml_node();

		/**
		 * Register signal handler for tracking config modifications
		 */
		void sigh(Signal_context_capability cap);

		/**
		 * Reload configuration
		 *
		 * \throw Invalid  if the new configuration has an invalid syntax
		 *
		 * This method is meant to be called as response to a signal
		 * received by the signal handler as registered via 'sigh()'.
		 */
		void reload();
};

#endif /* _INCLUDE__OS__CONFIG_H_ */

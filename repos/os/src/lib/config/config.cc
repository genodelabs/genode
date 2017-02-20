/*
 * \brief  Access to process configuration
 * \author Norman Feske
 * \date   2010-05-04
 *
 * \deprecated
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#define INCLUDED_BY_OS_CONFIG_CC
#include <os/config.h>
#undef INCLUDED_BY_OS_CONFIG_CC

using namespace Genode;

Xml_node _config_xml_node(Dataspace_capability config_ds)
{
	if (!config_ds.valid())
		throw Exception();

	return Xml_node(env_deprecated()->rm_session()->attach(config_ds),
	                Genode::Dataspace_client(config_ds).size());
}


/**
 * Fallback XML node used if the configuration is broken
 */
static Xml_node fallback_config_xml()
{
	return Xml_node("<config/>");
}


void Config::reload()
{
	if (!this)
		return;

	try {
		/* re-acquire dataspace from ROM session */
		if (_config_ds.valid())
			env_deprecated()->rm_session()->detach(_config_xml.addr());

		_config_ds = _config_rom.dataspace();

		/* re-initialize XML node with new config data */
		_config_xml = _config_xml_node(_config_ds);

	} catch (Genode::Xml_node::Invalid_syntax) {
		Genode::error("config ROM has invalid syntax");
		_config_xml = fallback_config_xml();
	}
}


Xml_node Config::xml_node()
{
	if (!this)
		return fallback_config_xml();

	return _config_xml;
}


void Config::sigh(Signal_context_capability cap)
{
	if (this)
		_config_rom.sigh(cap);
}


Config::Config()
:
	_config_rom(false, "config"),
	_config_ds(_config_rom.dataspace()),
	_config_xml(_config_xml_node(_config_ds))
{ }


Reconstructible<Config> &Genode::config()
{
	static bool config_failed = false;
	if (!config_failed) {
		try {
			static Reconstructible<Config> config_inst;
			return config_inst;
		} catch (Genode::Rom_connection::Rom_connection_failed) {
			Genode::error("Could not obtain config file");
		} catch (Genode::Xml_node::Invalid_syntax) {
			Genode::error("Config file has invalid syntax");
		} catch(...) {
			Genode::error("Config dataspace is invalid");
		}
	}
	/* do not try again to construct 'config_inst' */
	config_failed = true;
	class Config_construction_failed : Genode::Exception { };
	throw Config_construction_failed();
}


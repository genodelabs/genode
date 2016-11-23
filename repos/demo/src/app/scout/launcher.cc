/*
 * \brief  Pseudo launcher for the Genode version of Scout
 * \author Norman Feske
 * \date   2006-08-28
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <launchpad/launchpad.h>
#include <dataspace/capability.h>
#include <rom_session/connection.h>
#include <base/snprintf.h>
#include "elements.h"

using namespace Genode;
using namespace Scout;


/**********************************************
 ** Registry containing child configurations **
 **********************************************/

/**
 * The registry contains config dataspaces for given program names. It is
 * filled lazily as a side effect of 'Launcher::launch()'.
 */
class Config_registry
{
	private:

		struct Entry;
		List<Entry> _configs;

	public:

		/**
		 * Obtain configuration for specified program name from ROM module
		 * named '<prg_name>.config'
		 */
		Dataspace_capability config(char const *name);
};


struct Config_registry::Entry : List<Config_registry::Entry>::Element
{
	Dataspace_capability _init_dataspace(char const *name)
	{
		char buf[256];
		snprintf(buf, sizeof(buf), "%s.config", name);
		Rom_connection *config = 0;
		try {
			config = new (env()->heap()) Rom_connection(buf);
			return config->dataspace();
		}
		catch (...) { }
		return Dataspace_capability();
	}

	Dataspace_capability const dataspace;
	char                       name[128];

	Entry(char const *prg_name) : dataspace(_init_dataspace(prg_name))
	{
		strncpy(name, prg_name, sizeof(name));
	}
};


Dataspace_capability Config_registry::config(char const *name)
{
	/* lookup existing configuration */
	for (Entry *e = _configs.first(); e; e = e->next())
		if (strcmp(name, e->name) == 0)
			return e->dataspace;

	/* if lookup failed, create and register new config */
	Entry *entry = new (env()->heap()) Entry(name);
	_configs.insert(entry);

	return entry->dataspace;
}


/************************
 ** Launcher interface **
 ************************/


static Launchpad *launchpad_ptr;


static Launchpad &launchpad()
{
	if (!launchpad_ptr) {
		class Missing_launchpad_init_call { };
		throw Missing_launchpad_init_call();
	}

	return *launchpad_ptr;
}


void Launcher::init(Genode::Env &env)
{
	static Launchpad launchpad(env, env.ram().avail());
	launchpad_ptr = &launchpad;
}


void Launcher::launch()
{
	static Config_registry config_registry;

	launchpad().start_child(prg_name(), quota(),
	                        config_registry.config(prg_name().string()));
}

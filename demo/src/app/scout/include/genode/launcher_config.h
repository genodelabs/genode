/*
 * \brief  Genode-specific Launcher support
 * \author Norman Feske
 * \date   2009-08-25
 *
 * The generic Scout code uses the 'Launcher_config' as an opaque type.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SCOUT__GENODE__LAUNCHER_CONFIG__
#define _SCOUT__GENODE__LAUNCHER_CONFIG__

#include <dataspace/capability.h>

class Launcher_config
{
	private:

		Genode::Dataspace_capability _config_ds;

	public:

		/**
		 * Constructor
		 */
		Launcher_config(Genode::Dataspace_capability config_ds)
		: _config_ds(config_ds) { }

		Genode::Dataspace_capability config_ds() { return _config_ds; }
};

#endif /* _SCOUT__GENODE__LAUNCHER_CONFIG__ */

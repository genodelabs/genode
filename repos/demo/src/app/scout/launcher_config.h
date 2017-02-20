/*
 * \brief  Genode-specific Launcher support
 * \author Norman Feske
 * \date   2009-08-25
 *
 * The generic Scout code uses the 'Launcher_config' as an opaque type.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LAUNCHER_CONFIG_
#define _LAUNCHER_CONFIG_

#include <dataspace/capability.h>

namespace Scout { class Launcher_config; }

class Scout::Launcher_config
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

#endif /* _LAUNCHER_CONFIG_ */

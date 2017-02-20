/*
 * \brief  Support for launcher of the Genode programs via the Launchpad
 * \author Norman Feske
 * \date   2006-08-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <launchpad/launchpad.h>
#include "elements.h"
#include "launcher_config.h"

using namespace Scout;


void Launcher::launch()
{
	_launchpad->start_child(prg_name(), quota(),
	                        _config ? _config->config_ds()
	                                : Genode::Dataspace_capability());
}

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
#include "elements.h"

static Launchpad launchpad(Genode::env()->ram_session()->quota());


/************************
 ** Launcher interface **
 ************************/

void Launcher::launch()
{
	launchpad.start_child(prg_name(), quota(), Genode::Dataspace_capability());
}

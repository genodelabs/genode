/*
 * \brief  Service backend helper
 * \author Christian Helmuth
 * \date   2021-09-01
 *
 * This module stores the Genode environment reference for service shared
 * objects that require Genode connections (e.g., shared-clipboard reports
 * and ROMs).
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/env.h>

/* local includes */
#include <services/services.h>


static Genode::Env *genode_env;

Genode::Env & Services::env()         { return *genode_env; }
void Services::init(Genode::Env &env) { genode_env = &env; }

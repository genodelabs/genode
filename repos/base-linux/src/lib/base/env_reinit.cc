/*
 * \brief  Environment reinitialization
 * \author Norman Feske
 * \date   2016-04-29
 *
 * Support functions for implementing fork on Noux, which is not supported on
 * Linux.
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base-internal includes */
#include <base/internal/platform_env.h>


void Genode::Platform_env_base::reinit(Native_capability::Raw) { }


void Genode::Platform_env_base::reinit_main_thread(Capability<Region_map> &) { }

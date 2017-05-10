/*
 * \brief  Access to the component's initial thread capability
 * \author Norman Feske
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <deprecated/env.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/parent_cap.h>

Genode::Thread_capability Genode::main_thread_cap()
{
	return Genode::env_deprecated()->parent()->main_thread_cap();
}

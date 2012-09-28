/*
 * \brief  Information about the main thread
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;

Native_utcb *main_thread_utcb()
{
	return reinterpret_cast<Native_utcb *>(
	       Native_config::context_area_virtual_base() +
	       Native_config::context_area_virtual_size() - Nova::PAGE_SIZE_BYTE);
}


addr_t main_thread_running_semaphore() { return Nova::SM_SEL_EC; }

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

/**
 * Location of the main thread's UTCB, initialized by the startup code
 */
Nova::mword_t __main_thread_utcb;


Native_utcb *main_thread_utcb() { return (Native_utcb *)__main_thread_utcb; }


addr_t main_thread_running_semaphore() { return Nova::SM_SEL_EC; }

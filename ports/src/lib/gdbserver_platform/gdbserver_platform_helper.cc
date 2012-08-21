/*
 * \brief  Genode backend for GDBServer - helper functions
 * \author Christian Prochaska
 * \date   2011-07-07
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern "C" {
#define private _private
#include "server.h"
#include "linux-low.h"
#include "genode-low.h"
#define _private private
}

#include "cpu_session_component.h"
#include "gdb_stub_thread.h"

using namespace Genode;
using namespace Gdb_monitor;

extern Gdb_stub_thread *gdb_stub_thread();

bool get_current_thread_state(Thread_state &thread_state)
{
	Cpu_session_component *csc = gdb_stub_thread()->cpu_session_component();

	ptid_t ptid = ((struct inferior_list_entry*)current_inferior)->id;

	return !csc->state(csc->thread_cap(ptid.lwp), &thread_state);
}


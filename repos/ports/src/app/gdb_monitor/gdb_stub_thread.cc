/*
 * \brief  GDB stub thread implementation
 * \author Christian Prochaska
 * \date   2011-03-10
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/sleep.h>

#include "gdb_stub_thread.h"

using namespace Genode;
using namespace Gdb_monitor;

Gdb_stub_thread::Gdb_stub_thread()
:
	Thread<GDB_STUB_STACK_SIZE>("GDB server thread"),
	_cpu_session_component(0),
	_rm_session_component(0),
	_signal_handler_thread(&_exception_signal_receiver)
{
	_signal_handler_thread.start();
}


extern "C" int gdbserver_main(const char *port, void *gdb_stub_thread);


void Gdb_stub_thread::entry()
{
	gdbserver_main("/dev/terminal", this);

	sleep_forever();
};

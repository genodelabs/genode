/*
 * \brief  Generic dummy 'Native_cpu' setup
 * \author Christian Prochaska
 * \date   2016-05-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* GDB monitor includes */
#include "cpu_session_component.h"

Genode::Capability<Genode::Cpu_session::Native_cpu>
Gdb_monitor::Cpu_session_component::_setup_native_cpu() {
	return Genode::Capability<Genode::Cpu_session::Native_cpu>(); }

void Gdb_monitor::Cpu_session_component::_cleanup_native_cpu() { }

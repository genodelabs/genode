/*
 * \brief  Generic VM-connection implementation
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vm_session/connection.h>

using namespace Genode;

struct Vm_session::Native_vcpu { };

static Vm_session::Native_vcpu dummy;


void Vm_connection::Vcpu::run() { }


void Vm_connection::Vcpu::pause() { }


struct Genode::Vcpu_state { };

Vcpu_state & Vm_connection::Vcpu::state()
{
	static char dummy[sizeof(Vcpu_state)];

	return *(Vcpu_state *)dummy;
}


Vm_connection::Vcpu::Vcpu(Vm_connection &, Allocator &,
                          Vcpu_handler_base &, Exit_config const &)
:
	_native_vcpu(dummy)
{
}

/*
 * \brief  Generic VM-connection implementation
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \author Benjamin Lamowski
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vm_session/connection.h>

using namespace Genode;

struct Vm_session::Native_vcpu { };

static Vm_session::Native_vcpu dummy;

struct Genode::Vcpu_state { };

void Vm_connection::Vcpu::_with_state(With_state::Ft const &) { };

Vm_connection::Vcpu::Vcpu(Vm_connection &, Allocator &,
                          Vcpu_handler_base &, Exit_config const &)
:
	_native_vcpu(dummy)
{ }

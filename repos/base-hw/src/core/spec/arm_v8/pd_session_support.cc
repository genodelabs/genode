/*
 * \brief  Core implementation of the PD session interface
 * \author Stefan Kalkowski
 * \date   2020-07-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu/cpu_state.h>
#include <hw/spec/arm_64/psci_call.h>
#include <pd_session_component.h>

using namespace Genode;
using State = Genode::Pd_session::Managing_system_state;


State Pd_session_component::managing_system(State const & s)
{
	State ret;
	ret.r[0] = Hw::Psci_smc_functor::call(s.r[0], s.r[1], s.r[2], s.r[3]);
	return ret;
}


/***************************
 ** Dummy implementations **
 ***************************/

bool Pd_session_component::assign_pci(addr_t, uint16_t) { return true; }


void Pd_session_component::map(addr_t, addr_t) { }


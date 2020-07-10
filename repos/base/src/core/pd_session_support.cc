/*
 * \brief  Core implementation of the PD session interface
 * \author Norman Feske
 * \date   2016-01-13
 *
 * This dummy is used on all kernels with no IOMMU and managing system support.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core-local includes */
#include <pd_session_component.h>

using namespace Genode;

bool Pd_session_component::assign_pci(addr_t, uint16_t) { return true; }

void Pd_session_component::map(addr_t, addr_t) { }

using State = Genode::Pd_session::Managing_system_state;

State Pd_session_component::managing_system(State const &) { return State(); }

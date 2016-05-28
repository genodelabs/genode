/*
 * \brief  Core implementation of the PD session interface
 * \author Norman Feske
 * \date   2016-01-13
 *
 * This dummy is used on all kernels with no IOMMU support.
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core-local includes */
#include <pd_session_component.h>

using namespace Genode;

bool Pd_session_component::assign_pci(addr_t, uint16_t) { return false; }


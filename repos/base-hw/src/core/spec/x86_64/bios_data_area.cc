/*
 * \brief  Structure of the Bios Data Area after preparation through Bender
 * \author Martin Stein
 * \date   2015-07-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <bios_data_area.h>
#include <platform.h>

using namespace Genode;

addr_t Bios_data_area::_mmio_base_virt() { return Platform::mmio_to_virt(0); }

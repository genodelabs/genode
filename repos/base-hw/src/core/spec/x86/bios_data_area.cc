/*
 * \brief  Structure of the Bios Data Area after preparation through Bender
 * \author Martin Stein
 * \date   2015-07-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <bios_data_area.h>

using namespace Genode;

addr_t Bios_data_area::_mmio_base_virt() { return 0x1ff000; }

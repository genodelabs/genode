/*
 * \brief  Freescale TrustZone aware interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <board.h>
#include <platform.h>

using namespace Core;


Hw::Pic::Pic() : Mmio({(char *)Platform::mmio_to_virt(Board::IRQ_CONTROLLER_BASE), Mmio::SIZE}) { }

/*
 * \brief  Timer implementation for core
 * \author Stefan Kalkowski
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <board.h>
#include <timer.h>
#include <platform.h>

Genode::Timer::Timer()
: Genode::Mmio(Genode::Platform::mmio_to_virt(Board::EPIT_1_MMIO_BASE)) { }


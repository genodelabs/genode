/*
 * \brief   Board implementation specific to Cortex A9
 * \author  Stefan Kalkowski
 * \date    2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <board.h>
#include <platform.h>

using namespace Genode;

Cortex_a9::Board::Board()
: _l2_cache(Platform::mmio_to_virt(Board_base::PL310_MMIO_BASE)) {}

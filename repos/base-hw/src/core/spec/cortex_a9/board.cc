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

Board::L2_cache & Board::l2_cache()
{
	using namespace Genode;

	static L2_cache cache(Platform::mmio_to_virt(Board::PL310_MMIO_BASE));
	return cache;
}

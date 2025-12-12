/*
 * \brief  Platform driver - IO MMU interface
 * \author Johannes Schlatow
 * \date   2023-01-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <device.h>
#include <io_mmu.h>

bool Driver::Io_mmu_factory::matches(Device const &dev) {
	 return dev.type() == _type; }

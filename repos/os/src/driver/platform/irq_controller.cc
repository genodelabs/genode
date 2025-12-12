/*
 * \brief  Platform driver - IRQ controller interface
 * \author Johannes Schlatow
 * \date   2024-03-20
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <device.h>
#include <irq_controller.h>

bool Driver::Irq_controller_factory::matches(Device const &dev) {
	return dev.type() == _type; }

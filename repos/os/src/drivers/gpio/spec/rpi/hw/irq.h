/*
 * \brief  GPIO interrupt number
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-27
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__GPIO__SPEC__RPI__SPEC__HW__IRQ_H_
#define _DRIVERS__GPIO__SPEC__RPI__SPEC__HW__IRQ_H_

/* Genode includes */
#include <drivers/defs/rpi.h>

namespace Gpio
{
	enum { IRQ = Rpi::GPU_IRQ_BASE + 49 };
}

#endif /* _DRIVERS__GPIO__SPEC__RPI__SPEC__HW__IRQ_H_ */

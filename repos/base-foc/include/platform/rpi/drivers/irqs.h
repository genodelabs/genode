/*
 * \brief  RaspberryPI specific IRQ definitions
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-27
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__RPI__DRIVERS__IRQS_H_
#define _INCLUDE__PLATFORM__RPI__DRIVERS__IRQS_H_

/* Genode includes */
#include <drivers/board_base.h>

namespace Rpi_irqs
{
	enum {
		GPIO_IRQ = 49,
	};
}

#endif /* _INCLUDE__PLATFORM__RPI__DRIVERS__IRQS_H_ */


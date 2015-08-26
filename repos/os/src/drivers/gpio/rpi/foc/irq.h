/*
 * \brief  GPIO interrupt number
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-27
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IRQ_H_
#define _IRQ_H_

/* Genode includes */
#include <drivers/board_base.h>

namespace Gpio
{
	enum { IRQ = 49 };
}

#endif /* _IRQ_H_ */

/*
 * \brief  Lx_kit pending interrupt utility
 * \author Josef Soentgen
 * \date   2023-06-22
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__PENDING_IRQ_H_
#define _LX_KIT__PENDING_IRQ_H_

#include <util/fifo.h>

namespace Lx_kit {

	using namespace Genode;

	struct Pending_irq;
}


struct Lx_kit::Pending_irq : Genode::Fifo<Lx_kit::Pending_irq>::Element
{
	unsigned value;

	Pending_irq(unsigned value) : value (value) { }
};

#endif /* _LX_KIT__PENDING_IRQ_H_ */

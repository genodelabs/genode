/*
 * \brief  Freescale's TrustZone aware interrupt controller
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <board.h>

Hw::Pic::Pic()
:
	Mmio({(char *)Board::IRQ_CONTROLLER_BASE, Board::IRQ_CONTROLLER_SIZE})
{
	for (unsigned i = 0; i < NR_OF_IRQ; i++) {
		write<Intsec::Nonsecure>(!Board::secure_irq(i), i);
		if (!Board::secure_irq(i)) write<Priority>(0x80, i);
		write<Enclear::Clear_enable>(1, i);
	}
	write<Priomask::Mask>(0xff);
	Intctrl::access_t v = 0;
	Intctrl::Enable::set(v, 1);
	Intctrl::Nsen::set(v, 1);
	Intctrl::Nsen_mask::set(v, 1);
	write<Intctrl>(v);
}

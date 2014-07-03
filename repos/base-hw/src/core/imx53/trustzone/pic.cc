/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <pic.h>

using namespace Genode;

Pic::Pic() : Mmio(Board::TZIC_MMIO_BASE)
{
	_common_init();
	for (unsigned i = 0; i < NR_OF_IRQ; i++) {
		write<Intsec::Nonsecure>(0, i);
		write<Priority>(0, i);
	}
	write<Priomask::Mask>(0xff);
}


void Pic::unsecure(unsigned const i)
{
	if (i < NR_OF_IRQ) {
		write<Intsec::Nonsecure>(1, i);
		write<Priority>(0x80, i);
	}
}


void Pic::secure(unsigned const i)
{
	if (i < NR_OF_IRQ) {
		write<Intsec::Nonsecure>(0, i);
		write<Priority>(0, i);
	}
}

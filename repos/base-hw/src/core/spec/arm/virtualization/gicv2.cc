/*
 * \brief  Gicv2 with virtualization extensions
 * \author Stefan Kalkowski
 * \date   2019-09-02
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <platform.h>
#include <spec/arm/virtualization/gicv2.h>

using Board::Pic;


Pic::Gich::Gich()
:
	Mmio({(char *)Core::Platform::mmio_to_virt(Board::Cpu_mmio::IRQ_CONTROLLER_VT_CTRL_BASE), Mmio::SIZE})
{ }


bool Pic::ack_virtual_irq(Pic::Virtual_context & c)
{
	c.misr   = _gich.read<Gich::Gich_misr  >();
	c.vmcr   = _gich.read<Gich::Gich_vmcr  >();
	c.apr    = _gich.read<Gich::Gich_apr   >();
	c.eisr   = _gich.read<Gich::Gich_eisr0 >();
	c.elrsr  = _gich.read<Gich::Gich_elrsr0>();
	c.lr     = _gich.read<Gich::Gich_lr0   >();
	_gich.write<Gich::Gich_hcr>(0);

	if (c.eisr & 1) {
		c.lr    = 0;
		c.elrsr = 0xffffffff;
		c.misr  = 0;
		c.eisr  = 0;
		return true;
	}

	return false;
}


void Pic::insert_virtual_irq(Pic::Virtual_context & c, unsigned irq)
{
	enum { SPURIOUS = 1023 };

	if (irq != SPURIOUS && !c.lr) {
		c.elrsr &= 0x7ffffffe;
		c.lr     = irq | 1 << 28 | 1 << 19;
	}

	_gich.write<Gich::Gich_misr  >(c.misr);
	_gich.write<Gich::Gich_vmcr  >(c.vmcr);
	_gich.write<Gich::Gich_apr   >(c.apr);
	_gich.write<Gich::Gich_elrsr0>(c.elrsr);
	_gich.write<Gich::Gich_lr0   >(c.lr);
	_gich.write<Gich::Gich_hcr>(0b1);
}

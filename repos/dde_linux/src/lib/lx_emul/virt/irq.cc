/*
 * \brief  Dummy IRQ emulation for virt_linux
 * \author Martin Stein
 * \author Christian Helmuth
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/irq.h>
#include <lx_kit/env.h>


extern "C" void lx_emul_irq_unmask(unsigned int ) { }


extern "C" void lx_emul_irq_mask(unsigned int ) { }


extern "C" void lx_emul_irq_eoi(unsigned int ) { }


extern "C" unsigned int lx_emul_irq_last()
{
	return Lx_kit::env().last_irq;
}

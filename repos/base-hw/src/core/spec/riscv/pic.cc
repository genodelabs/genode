/*
 * \brief   RISC-V PIC initialization
 * \author  Sebastian Sumpf
 * \date    2021-03-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <board.h>
#include <pic.h>
#include <platform.h>

Board::Pic::Pic(Global_interrupt_controller &)
:
	_plic({(char *)Core::Platform::mmio_to_virt(Board::PLIC_BASE), Board::PLIC_SIZE})
{
	/* enable external interrupts */
	enum { SEIE = 0x200 };
	Hw::Riscv_cpu::Sie external_interrupt(SEIE);
}

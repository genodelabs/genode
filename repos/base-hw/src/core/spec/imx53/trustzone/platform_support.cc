/*
 * \brief   Specific core implementations
 * \author  Stefan Kalkowski
 * \date    2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <drivers/trustzone.h>

/* core includes */
#include <platform.h>
#include <board.h>
#include <pic.h>
#include <cpu.h>
#include <trustzone.h>
#include <csu.h>

#include <base/internal/unmanaged_singleton.h>

using namespace Genode;

/* monitor exception vector address */
extern int _mon_kernel_entry;

bool secure_irq(unsigned const i)
{
	using namespace Csu_config;
	if (i == Board::EPIT_1_IRQ)                           return true;
	if (i == Board::EPIT_2_IRQ)                           return true;
	if (i == Board::I2C_2_IRQ)                            return SECURE_I2C;
	if (i == Board::I2C_3_IRQ)                            return SECURE_I2C;
	if (i == Board::SDHC_IRQ)                             return SECURE_ESDHC;
	if (i >= Board::GPIO1_IRQL && i <= Board::GPIO4_IRQH) return SECURE_GPIO;
	if (i >= Board::GPIO5_IRQL && i <= Board::GPIO7_IRQH) return SECURE_GPIO;
	return false;
}


void Kernel::init_trustzone(Pic & pic)
{
	using namespace Genode;

	/* set exception vector entry */
	Cpu::mon_exception_entry_at((Genode::addr_t)&_mon_kernel_entry);

	/* enable coprocessor 10 + 11 access for TZ VMs */
	Cpu::Nsacr::access_t v = 0;
	Cpu::Nsacr::Cpnsae10::set(v, 1);
	Cpu::Nsacr::Cpnsae11::set(v, 1);
	Cpu::Nsacr::write(v);

	/* configure non-secure interrupts */
	for (unsigned i = 0; i < Pic::NR_OF_IRQ; i++) {
		if (!secure_irq(i)) { pic.unsecure(i); } }

	/* configure central security unit */
	Genode::Csu csu(Board::CSU_BASE);
}


Cpu::User_context::User_context() { cpsr = Psr::init_user_with_trustzone(); }

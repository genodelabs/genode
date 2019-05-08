/*
 * \brief   Specific i.MX53 bootstrap implementations
 * \author  Stefan Kalkowski
 * \date    2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <drivers/defs/imx53_trustzone.h>
#include <spec/arm/imx_aipstz.h>
#include <spec/arm/imx_csu.h>

using namespace Board;

bool Board::secure_irq(unsigned i)
{
	if (i == EPIT_1_IRQ) return true;
	if (i == EPIT_2_IRQ) return true;
	if (i == I2C_2_IRQ)  return true;
	if (i == I2C_3_IRQ)  return true;
	if (i == SDHC_IRQ)   return true;

	if (i >= GPIO1_IRQL && i <= GPIO4_IRQH) return true;
	if (i >= GPIO5_IRQL && i <= GPIO7_IRQH) return true;
	return false;
}


Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { Trustzone::SECURE_RAM_BASE,
                                    Trustzone::SECURE_RAM_SIZE }),
  core_mmio(Memory_region { UART_1_MMIO_BASE, UART_1_MMIO_SIZE },
            Memory_region { EPIT_1_MMIO_BASE, EPIT_1_MMIO_SIZE },
            Memory_region { IRQ_CONTROLLER_BASE, IRQ_CONTROLLER_SIZE },
            Memory_region { CSU_BASE, CSU_SIZE })
{
	Aipstz aipstz_1(AIPS_1_MMIO_BASE);
	Aipstz aipstz_2(AIPS_2_MMIO_BASE);

	/* set exception vector entry */
	Cpu::Mvbar::write(Hw::Mm::system_exception_vector().base);

	/* enable coprocessor 10 + 11 access for TZ VMs */
	Cpu::Nsacr::access_t v = 0;
	Cpu::Nsacr::Cpnsae10::set(v, 1);
	Cpu::Nsacr::Cpnsae11::set(v, 1);
	Cpu::Nsacr::write(v);

	/* configure central security unit */
	Csu csu(CSU_BASE, false, true, false, true);
}

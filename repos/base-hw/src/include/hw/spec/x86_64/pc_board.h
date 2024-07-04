/*
 * \brief   PC specific board definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__X86_64__PC_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__X86_64__PC_BOARD_H_

#include <drivers/uart/x86_pc.h>
#include <hw/spec/x86_64/acpi_rsdp.h>
#include <hw/spec/x86_64/framebuffer.h>

namespace Hw::Pc_board {

	struct Boot_info;
	struct Serial;
	enum Dummies { UART_BASE, UART_CLOCK };

	/**
	 * The constant 'NR_OF_CPUS' defines the _maximum_ of cpus currently
	 * supported on x86. The actual number is detected at booting.
	 */
	static constexpr Genode::size_t NR_OF_CPUS = 256;
}


struct Hw::Pc_board::Serial : Genode::X86_uart
{
	Serial(Genode::addr_t, Genode::size_t, unsigned);
};


struct Hw::Pc_board::Boot_info
{
	Acpi_rsdp      acpi_rsdp        { };
	Framebuffer    framebuffer      { };
	Genode::addr_t efi_system_table { 0 };
	Genode::addr_t acpi_fadt        { 0 };

	Boot_info() {}
	Boot_info(Acpi_rsdp    const &acpi_rsdp,
	          Framebuffer  const &fb)
	: acpi_rsdp(acpi_rsdp), framebuffer(fb) {}
};

#endif /* _SRC__BOOTSTRAP__SPEC__X86_64__BOARD_H_ */

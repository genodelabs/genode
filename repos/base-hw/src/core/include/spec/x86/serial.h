/*
 * \brief  Serial output driver for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

/* Genode includes */
#include <drivers/uart/x86_uart_base.h>
#include <util/mmio.h>
#include <unmanaged_singleton.h>

namespace Genode
{
	enum { BDA_MMIO_BASE_VIRT = 0x1ff000 };

	class Bios_data_area;
	class Serial;

	Bios_data_area * bda();
}

class Genode::Bios_data_area : Mmio
{
	friend Unmanaged_singleton_constructor;

	private:

		struct Serial_base_com1 : Register<0x400, 16> { };
		struct Equipment        : Register<0x410, 16>
		{
			struct Serial_count : Bitfield<9, 3> { };
		};

		/*
		 * Constructor
		 *
		 * The BDA page must be mapped already (see crt0_translation_table.s).
		 */
		Bios_data_area() : Mmio(BDA_MMIO_BASE_VIRT) { }

	public:

		/**
		 * Obtain I/O ports of COM interfaces from BDA
		 */
		addr_t serial_port() const
		{
			Equipment::access_t count = read<Equipment::Serial_count>();
			return count ? read<Serial_base_com1>() : 0;
		}

		/**
		 * Return BDA singleton
		 */
		static Bios_data_area * singleton() {
			return unmanaged_singleton<Bios_data_area>(); }
};

/**
 * Serial output driver for core
 */
class Genode::Serial : public X86_uart_base
{
	private:

		enum { CLOCK_UNUSED = 0 };

	public:

		Serial(unsigned baud_rate)
		:
			X86_uart_base(Bios_data_area::singleton()->serial_port(),
			              CLOCK_UNUSED, baud_rate)
		{ }
};

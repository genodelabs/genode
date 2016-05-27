/*
 * \brief  Structure of the Bios Data Area after preparation through Bender
 * \author Martin Stein
 * \date   2015-07-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPEC__X86__BIOS_DATA_AREA_H_
#define _INCLUDE__SPEC__X86__BIOS_DATA_AREA_H_

/* Genode includes */
#include <util/mmio.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

namespace Genode { class Bios_data_area; }

class Genode::Bios_data_area : Mmio
{
	friend Unmanaged_singleton_constructor;

	private:

		struct Serial_base_com1 : Register<0x400, 16> { };
		struct Equipment        : Register<0x410, 16>
		{
			struct Serial_count : Bitfield<9, 3> { };
		};

		static addr_t _mmio_base_virt();

		Bios_data_area() : Mmio(_mmio_base_virt()) { }

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

#endif /* _INCLUDE__SPEC__X86__BIOS_DATA_AREA_H_ */

/*
 * \brief  Specific bootstrap implementations
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2014-02-25
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <spec/arm/imx_aipstz.h>

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_BASE, RAM_SIZE }),
  core_mmio(Memory_region { UART_BASE,
                            UART_SIZE },
            Memory_region { CORTEX_A9_PRIVATE_MEM_BASE,
                            CORTEX_A9_PRIVATE_MEM_SIZE },
            Memory_region { PL310_MMIO_BASE,
                            PL310_MMIO_SIZE })
{
	Aipstz aipstz_1(AIPS_1_MMIO_BASE);
	Aipstz aipstz_2(AIPS_2_MMIO_BASE);

	unsigned num_values = sizeof(initial_values) / (2*sizeof(unsigned long));
	for (unsigned i = 0; i < num_values; i++)
		*((volatile unsigned long*)initial_values[i][0]) = initial_values[i][1];
}


bool Board::Cpu::errata(Board::Cpu::Errata err) {
	return (err == ARM_764369) ? true : false; }


void Board::Cpu::wake_up_all_cpus(void * const entry)
{
	struct Src : Genode::Mmio
	{
		struct Scr  : Register<0x0,  32>
		{
			struct Core_1_reset  : Bitfield<14,1> {};
			struct Core_2_reset  : Bitfield<15,1> {};
			struct Core_3_reset  : Bitfield<16,1> {};
			struct Core_1_enable : Bitfield<22,1> {};
			struct Core_2_enable : Bitfield<23,1> {};
			struct Core_3_enable : Bitfield<24,1> {};
		};
		struct Gpr1 : Register<0x20, 32> {}; /* ep core 0 */
		struct Gpr3 : Register<0x28, 32> {}; /* ep core 1 */
		struct Gpr5 : Register<0x30, 32> {}; /* ep core 2 */
		struct Gpr7 : Register<0x38, 32> {}; /* ep core 3 */

		Src(void * const entry) : Genode::Mmio(SRC_MMIO_BASE)
		{
			write<Gpr3>((Gpr3::access_t)entry);
			write<Gpr5>((Gpr5::access_t)entry);
			write<Gpr7>((Gpr7::access_t)entry);
			Scr::access_t v = read<Scr>();
			Scr::Core_1_enable::set(v,1);
			Scr::Core_1_reset::set(v,1);
			Scr::Core_2_enable::set(v,1);
			Scr::Core_2_reset::set(v,1);
			Scr::Core_3_enable::set(v,1);
			Scr::Core_3_reset::set(v,1);
			write<Scr>(v);
		}
	};

	Src src(entry);
}

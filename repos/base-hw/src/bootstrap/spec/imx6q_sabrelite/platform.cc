/*
 * \brief  Specific bootstrap implementations
 * \author Stefan Kalkowski
 * \date   2019-01-05
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <spec/arm/imx_aipstz.h>

using namespace Board;

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_BASE, RAM_SIZE }),
  core_mmio(Memory_region { UART_2_MMIO_BASE,
                            UART_2_MMIO_SIZE },
            Memory_region { CORTEX_A9_PRIVATE_MEM_BASE,
                            CORTEX_A9_PRIVATE_MEM_SIZE },
            Memory_region { PL310_MMIO_BASE,
                            PL310_MMIO_SIZE })
{
	Aipstz aipstz_1(AIPS_1_MMIO_BASE);
	Aipstz aipstz_2(AIPS_2_MMIO_BASE);

	static volatile unsigned long initial_values[][2] {
		// (IOMUX Controller)
		{ 0x20e0004, 0x48613005 },
		{ 0x20e0008, 0x0 },
		{ 0x20e000c, 0x1e00040 },
		{ 0x20e0020, 0xfffd4000 },
		{ 0x20e0030, 0xf004490 },
		{ 0x20e0034, 0x593e4a4 },
		{ 0x20e004c, 0x3 },
		{ 0x20e0050, 0x3 },
		{ 0x20e0054, 0x3 },
		{ 0x20e015c, 0x0 },
		{ 0x20e0160, 0x0 },
		{ 0x20e0164, 0x0 },
		{ 0x20e0168, 0x0 },
		{ 0x20e0170, 0x0 },
		{ 0x20e0174, 0x0 },
		{ 0x20e0178, 0x0 },
		{ 0x20e017c, 0x0 },
		{ 0x20e0180, 0x0 },
		{ 0x20e0184, 0x0 },
		{ 0x20e0188, 0x0 },
		{ 0x20e018c, 0x0 },
		{ 0x20e0190, 0x0 },
		{ 0x20e0194, 0x0 },
		{ 0x20e0198, 0x0 },
		{ 0x20e019c, 0x0 },
		{ 0x20e01a0, 0x0 },
		{ 0x20e01a4, 0x0 },
		{ 0x20e01a8, 0x0 },
		{ 0x20e01ac, 0x0 },
		{ 0x20e01b0, 0x0 },
		{ 0x20e01b4, 0x0 },
		{ 0x20e01b8, 0x0 },
		{ 0x20e01bc, 0x0 },
		{ 0x20e01c0, 0x0 },
		{ 0x20e01c4, 0x0 },
		{ 0x20e01c8, 0x0 },
		{ 0x20e01cc, 0x0 },
		{ 0x20e0208, 0x2 },
		{ 0x20e020c, 0x2 },
		{ 0x20e0218, 0x2 },
		{ 0x20e0220, 0x0 },
		{ 0x20e0224, 0x3 },
		{ 0x20e0230, 0x11 },
		{ 0x20e02b8, 0x0 },
		{ 0x20e02f4, 0x0 },
		{ 0x20e033c, 0x2 },
		{ 0x20e0344, 0x3 },
		{ 0x20e0348, 0x2 },
		{ 0x20e035c, 0x3 },
		{ 0x20e0360, 0x130b0 },
		{ 0x20e0364, 0x110b0 },
		{ 0x20e0368, 0x130b0 },
		{ 0x20e036c, 0x10030 },
		{ 0x20e0370, 0x10030 },
		{ 0x20e0374, 0x10030 },
		{ 0x20e0378, 0x10030 },
		{ 0x20e037c, 0x10030 },
		{ 0x20e0388, 0x10030 },
		{ 0x20e03b0, 0xb1 },
		{ 0x20e03bc, 0xb0 },
		{ 0x20e03c0, 0xb0 },
		{ 0x20e0470, 0x10 },
		{ 0x20e0474, 0x10 },
		{ 0x20e0478, 0x10 },
		{ 0x20e047c, 0x10 },
		{ 0x20e0484, 0x10 },
		{ 0x20e0488, 0x10 },
		{ 0x20e048c, 0x10 },
		{ 0x20e0490, 0x10 },
		{ 0x20e0494, 0x10 },
		{ 0x20e0498, 0x10 },
		{ 0x20e049c, 0x10 },
		{ 0x20e04a0, 0x10 },
		{ 0x20e04a4, 0x10 },
		{ 0x20e04a8, 0x10 },
		{ 0x20e04ac, 0x10 },
		{ 0x20e04b0, 0x10 },
		{ 0x20e04b4, 0x10 },
		{ 0x20e04b8, 0x10 },
		{ 0x20e04bc, 0x10 },
		{ 0x20e04c0, 0x10 },
		{ 0x20e04c4, 0x10 },
		{ 0x20e04c8, 0x10 },
		{ 0x20e04cc, 0x10 },
		{ 0x20e04d0, 0x10 },
		{ 0x20e04d4, 0x10 },
		{ 0x20e04d8, 0x10 },
		{ 0x20e04dc, 0x10 },
		{ 0x20e04e0, 0x10 },
		{ 0x20e04e4, 0x100b0 },
		{ 0x20e04e8, 0x100b0 },
		{ 0x20e0508, 0x100b0 },
		{ 0x20e05f0, 0x30b0 },
		{ 0x20e05f4, 0x17059 },
		{ 0x20e0600, 0xb1 },
		{ 0x20e061c, 0x30b0 },
		{ 0x20e069c, 0x1f0b0 },
		{ 0x20e06a4, 0x10059 },
		{ 0x20e06e0, 0x10059 },
		{ 0x20e0724, 0x1b0b1 },
		{ 0x20e072c, 0x1b0b1 },
		{ 0x20e0730, 0x1b0b1 },
		{ 0x20e0744, 0x130b0 },
		{ 0x20e07c4, 0x1 },
		{ 0x20e0944, 0x1 },
		// (Global Power Controller)
		{ 0x20dc008, 0x70f7f01b },
		{ 0x20dc00c, 0xff79b60f },
		{ 0x20dc010, 0xfffe0003 },
		{ 0x20dc014, 0xfef7f9ff },
		// (Power Management Unit)
		{ 0x20c8140, 0x4c0013 },
		{ 0x20c8150, 0x4010088 },
		{ 0x20c8160, 0x8000040b },
		{ 0x20c8170, 0xff672f67 },
		// (Clock Controller Module)
		{ 0x20c4018, 0x10204 },
		{ 0x20c402c, 0x7348c1 },
		{ 0x20c4030, 0x33e71f92 },
		{ 0x20c4034, 0x12088 },
		{ 0x20c4038, 0x12090 },
		{ 0x20c4054, 0x78 },
		{ 0x20c4060, 0x10e0101 },
		{ 0x20c4064, 0x2fe62 },
		{ 0x20c4068, 0xc03f0f },
		{ 0x20c406c, 0x30fc00 },
		{ 0x20c4070, 0x3ff0033 },
		{ 0x20c4074, 0x3f3300c3 },
		{ 0x20c4078, 0xc303 },
		{ 0x20c4080, 0xf03 },
		{ 0x20c8010, 0x80003040 },
		{ 0x20c8070, 0x1006 },
		{ 0x20c80a0, 0x1028 },
		{ 0x20c80b0, 0x0 },
		{ 0x20c80c0, 0xf4240 },
		{ 0x20c80e0, 0x80182001 },
		{ 0x20c80f0, 0xd3d150cc },
		{ 0x20c8100, 0x5018d0db }
	};

	unsigned num_values = sizeof(initial_values) / (2*sizeof(unsigned long));
	for (unsigned i = 0; i < num_values; i++)
		*((volatile unsigned long*)initial_values[i][0]) = initial_values[i][1];
}


bool Bootstrap::Cpu::errata(Bootstrap::Cpu::Errata err) {
	return (err == ARM_764369) ? true : false; }


void Bootstrap::Cpu::wake_up_all_cpus(void * const entry)
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

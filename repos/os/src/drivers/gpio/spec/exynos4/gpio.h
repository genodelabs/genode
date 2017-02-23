/*
 * \brief  Odroid-x2 GPIO definitions
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-03
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GPIO_H_
#define _GPIO_H_

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <util/mmio.h>

#include <base/printf.h>

namespace Gpio {
	class Reg;

	using namespace Genode;
}

struct Gpio::Reg : Attached_io_mem_dataspace, Mmio
{
	struct Regs : Genode::Mmio
	{
		struct Con : Register<0x00, 32> {};
		struct Dat : Register<0x04, 8>  {};

		Regs(Genode::addr_t base) : Genode::Mmio(base) {}

		void set_con(unsigned int con) { write<Con>(con); }
		void set_dat(unsigned int dat) { write<Dat>(dat); }
		unsigned int get_con() { return read<Con>();}
		unsigned int get_dat() { return read<Dat>();}
	};

	struct Irq_regs : Genode::Mmio
	{
		struct Int_con : Register <0x00,32>
		{
			struct Con0 : Bitfield<0,  3> {};
			struct Con1 : Bitfield<4,  3> {};
			struct Con2 : Bitfield<8,  3> {};
			struct Con3 : Bitfield<12, 3> {};
			struct Con4 : Bitfield<16, 3> {};
			struct Con5 : Bitfield<20, 3> {};
			struct Con6 : Bitfield<24, 3> {};
			struct Con7 : Bitfield<28, 3> {};
		};

		Irq_regs(Genode::addr_t base) : Genode::Mmio(base) {}

		void enable_triggers(unsigned gpio, unsigned value)
		{
			write<Int_con>(0);
			switch(gpio) {
				case 0: write<Int_con::Con0>(value); break;
				case 1: write<Int_con::Con1>(value); break;
				case 2: write<Int_con::Con2>(value); break;
				case 3: write<Int_con::Con3>(value); break;
				case 4: write<Int_con::Con4>(value); break;
				case 5: write<Int_con::Con5>(value); break;
				case 6: write<Int_con::Con6>(value); break;
				case 7: write<Int_con::Con7>(value); break;
				default: PWRN("Not is valid irq con!");
			}
		}
	};

	Reg(Genode::Env &env, addr_t base, size_t size)
	: Attached_io_mem_dataspace(env, base, size),
	  Mmio((addr_t)local_addr<Reg>()) { }

	void set_direction(int gpio, bool input, Genode::off_t offset)
	{
		Regs _reg((Genode::addr_t)local_addr<void>() + offset);
		unsigned int value;
		int id = (input ? 0 : 0x1);
		value = _reg.get_con();
		value &= ~(0xf << (gpio << 2));
		value |=  (id << (gpio << 2));
		_reg.set_con(value);
	}

	void write_pin(unsigned gpio, bool level, Genode::off_t offset)
	{
		Regs _reg((Genode::addr_t)local_addr<void>() + offset);
		unsigned int value;
		value = _reg.get_dat();
		value &= ~(0x1 << gpio);
		if (level)
			value |= 0x1 << gpio;
		_reg.set_dat(value);
	}

	bool read_pin(unsigned gpio, Genode::off_t offset)
	{
		Regs _reg((Genode::addr_t)local_addr<void>() + offset);
		return (_reg.get_dat() & (1 << gpio)) !=0;
	}

	void set_enable_triggers(unsigned gpio, Genode::off_t offset,unsigned value)
	{
		Irq_regs _irq_regs((Genode::addr_t)local_addr<void>() + offset);
		_irq_regs.enable_triggers(gpio,value);
	}
};


enum {
	MAX_BANKS = 48,
	MAX_PINS  = 361
};

enum Irqs_triggers {
	LOW     = 0x0,
	HIGH    = 0x1,
	FALLING = 0x2,
	RISING  = 0x3,
	BOTH    = 0x4
};

const int _bank_sizes[MAX_PINS] = {
	/* TODO check value of registes type ETC. */
	/* GPIO Part1 */
	/* GPA0  GPA1  GPB  GPC0 GPC1  GPD0  GPD1  GPF0  GPF1  GPF2  GPF3  ETC1  GPJ0  GPJ1 */
	   8,    6,    8,   5,   5,    4,    4,    8,    8,    8,    6,    6,    8,    5,
	/* GPIO Part2 */   /* index 14 */
	/* GPK0  GPK1 GPK2 GPK3  GPL0  GPL1  GPL2  GPY0  GPY1  GPY2  GPY3  GPY4  GPY5 GPY6 */
	   7,    7,   7,   7,    7,    2,    8,    6,    4,    6,    8,    8,    8,   8,
	/* ETC0   ETC6 GPM0  GPM1 GPM2  GPM3  GPM4  GPX0  GPX1  GPX2  GPX3 */
	   6,     8,   8,    7,   5,    8,    8,    8,    8,    8,    8,   /* index 35,36,37 */
	/* GPIO Part3 */
	/* GPZ */   /* index 39 */
	   7,
	/* GPIO Part4 */  //index 40
	/* GPV0   GPV1  ETC7  GPV2  GPV3  ETC8 GPV4 */
	   8,     8,    2,    8,    8,    2,    8
};

const Genode::off_t _bank_offset[MAX_BANKS]=
{
		/* Part1 */
		/* GPA0    GPA1     GPB    GPC0    GPC1    GPD0    GPD1    GPF0    GPF1    GPF2    GPF3    ETC1    GPJ0    GPJ1 */
		0x0000,	0x0020, 0x0040, 0x0060, 0x0080, 0x00A0, 0x00C0, 0x0180, 0x01A0, 0x01C0, 0x01E0, 0x0228, 0x0240, 0x0260,
		/* Part2 */
		/* GPK0    GPK1    GPK2    GPK3    GPL0    GPL1    GPL2    GPY0    GPY1    GPY2    GPY3    GPY4    GPY5    GPY6 */
		0x0040, 0x0060, 0x0080, 0x00A0, 0x00C0, 0x00E0, 0x0100, 0x0120, 0x0140, 0x0160, 0x0180, 0x01A0, 0x01C0, 0x01E0,
		/* ETC0    ETC6    GPM0    GPM1    GPM2    GPM3    GPM4    GPX0    GPX1    GPX2    GPX3 */
		0x0208, 0x0228, 0x0260, 0x0280, 0x02A0, 0x02C0, 0x02E0, 0x0C00, 0x0C20, 0x0C40, 0x0C60,
		/* Part3 */
		0x0000, /*GPZ */
		/* Part4 */
		/* GPV0    GPV1    ETC7    GPV2    GPV3    ETC8    GPV4 */
		0x0000,	0x0020, 0x0048, 0x0060, 0x0080, 0x00A8, 0x00C0,
};

const Genode::off_t _irq_offset[MAX_BANKS]=
{
		/* Bank 1 irq */
		/* con1  con2    con3    con4    con5    con6    con7    con13   con14   con15   con16  ETC  con21   con22 */
		0x0700,  0x0704, 0x0708, 0x070C, 0x0710, 0x0714, 0x0718, 0x0730, 0x0734, 0x0738, 0x073C, -1, 0x0740, 0x0744,
		/* Bank 2 irq */
		/* con23 con24   con25   con26   con27   con28   con29 */
		0x0708, 0x070C, 0x0710, 0x0714, 0x0718,	0x071C, 0x0720, -1, -1, -1, -1, -1, -1, -1,
		/* con8    con9    con10   con11   con12   x0       x1      x2      x3 */
		   -1,     -1,     0x0724, 0x0728, 0x072C, 0x0730,  0x0734, 0x0E00, 0x0E04, 0x0E08, 0x0E0C, //TODO Check values de x0-x3.

		/* Bank 3 irq */
		/* con50 */
		0x0700,
		/* Bank 4 irq */
		/* con30    con31           con32   con33           con34 */
		   0x0700,  0x0704,   -1,   0x0708, 0x070C,   -1,   0x0710,
};

#endif /* _GPIO_H_ */

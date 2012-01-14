/*
 * \brief  Basic test for MMIO access framework
 * \author Christian Helmuth
 * \author Martin Stein
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/mmio.h>
#include <base/printf.h>

using namespace Genode;

/**
 * Assume this one is a cpu register, accessed by special ops
 */
static uint16_t cpu_state;

/**
 * Assume this is a MMIO region
 */
enum{ MMIO_SIZE = 8 };
static uint8_t mmio_mem[MMIO_SIZE]; 

/**
 * Exemplary highly structured type for accessing 'cpu_state'
 */
struct Cpu_state : Register<uint16_t>
{
	struct Mode : Subreg<0,4>
	{
		enum {
			KERNEL  = 0b1000,
			USER    = 0b1001,
			MONITOR = 0b1010,
		};
	};
	struct A   : Subreg<6,1> { };
	struct B   : Subreg<8,1> { };
	struct C   : Subreg<10,1> { };
	struct Irq : Subreg<12,3> { };

	struct Invalid_bit  : Subreg<18,1> { };
	struct Invalid_area : Subreg<15,4> { };

	inline static storage_t read() { return cpu_state; }

	inline static void write(storage_t & v) { cpu_state = v; }
};

struct A : public Mmio {

	A(addr_t const base) : Mmio(base) { }

};

/**
 * Exemplary MMIO region type
 */
struct Test_mmio : public Mmio
{
	Test_mmio(addr_t const base) : Mmio(base) { }

	struct Reg : Register<0x04, uint8_t> 
	{ 
		struct Bit_1 : Subreg<0,1> { };
		struct Area  : Subreg<1,3>
		{
			enum { 
				VALUE_1       = 3, 
				VALUE_2       = 4, 
				VALUE_3       = 5, 
			};
		};
		struct Bit_2            : Subreg<4,1> { };
		struct Invalid_bit      : Subreg<8,1> { };
		struct Invalid_area     : Subreg<6,8> { };
		struct Overlapping_area : Subreg<0,6> { };
	};

	struct Array : Reg_array<0x2, uint16_t, 10, 2> 
	{
		struct A : Subreg<0,1> { };
		struct B : Subreg<1,2> { };
		struct C : Subreg<3,1> { };
		struct D : Subreg<1,3> { };
	};
};
/*  little endian                                                      LSB --> MSB  */
/*  big endian                                                         MSB <-- LSB  */
/*  address    0x0     0x1     0x2     0x3     0x4     0x5     0x6     0x7     0x8  */
/*  bits       0   4   8   12  16  20  24  28  32  36  40  44  48  52  56  60  64   */
/*  bit        |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||    */
/*  4bit       |   |   |   |   |R0 |R1 |R2 |R3 |R4 |R5 |-- |-- |   |   |   |   |    */
/*  8bit (byte)|       |       |       |       |       |       |       |       |    */
/*  16bit      |               |Int0           |Int1           |               |    */
/*  32bit      |                               |                               |    */
/*  64bit      |                                                               |    */

/**
 * Print out memory content hexadecimal
 */
void dump_mem(uint8_t * base, size_t size) 
{
	addr_t top = (addr_t)base + size;
	for(; (addr_t)base < top;) { 
		printf("%2X ", *(uint8_t *)base);
		base = (uint8_t *)((addr_t)base + sizeof(uint8_t));
	}
}


/**
 * Zero-fill memory region
 */
void zero_mem(uint8_t * base, size_t size) 
{
	addr_t top = (addr_t)base + size;
	for(; (addr_t)base < top;) { 
		*base = 0;
		base = (uint8_t *)((addr_t)base + sizeof(uint8_t));
	}
}


/**
 * Compare content of two memory regions
 */
int compare_mem(uint8_t * base1, uint8_t * base2, size_t size)
{
	addr_t top = (addr_t)base1 + size;
	for(; (addr_t)base1 < top;) {
		if(*base1 != *base2) return -1;
		base1 = (uint8_t *)((addr_t)base1 + sizeof(uint8_t));
		base2 = (uint8_t *)((addr_t)base2 + sizeof(uint8_t));
	}
	return 0;
}


/**
 * End a failed test
 */
int test_failed(unsigned test_id)
{
	PERR("Test ended, test %i failed", test_id);
	printf("  mmio_mem:  0x ");
	dump_mem(mmio_mem, sizeof(mmio_mem));
	printf("\n  cpu_state: 0x%4X\n", cpu_state);
	return -1;
}


int main()
{
	/**********************************
	 ** Genode::Mmio::Register tests **
	 **********************************/

	/**
	 * Init fake MMIO
	 */
	Test_mmio mmio((addr_t)&mmio_mem[0]);

	/**
	 * Test 1, read/write whole reg, use 'Subreg::bits' with overflowing values
	 */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::Reg>(Test_mmio::Reg::Bit_1::bits(7) |
	                           Test_mmio::Reg::Area::bits(10) |
	                           Test_mmio::Reg::Bit_2::bits(9) );

	static uint8_t mmio_cmpr_1[MMIO_SIZE] = {0,0,0,0,0b00010101,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_1, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg>() != 0x15)
	{ return test_failed(1); }

	/**
	 * Test 2, read/write bit appropriately
	 */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::Reg::Bit_1>(1);

	static uint8_t mmio_cmpr_2[MMIO_SIZE] = {0,0,0,0,0b00000001,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_2, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Bit_1>() != 1) 
	{ return test_failed(2); }

	/**
	 * Test 3, read/write bit overflowing
	 */
	mmio.write<Test_mmio::Reg::Bit_2>(0xff);

	static uint8_t mmio_cmpr_3[MMIO_SIZE] = {0,0,0,0,0b00010001,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_3, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Bit_2>() != 1) 
	{ return test_failed(3); }

	/**
	 * Test 4, read/write bitarea appropriately
	 */
	mmio.write<Test_mmio::Reg::Area>(Test_mmio::Reg::Area::VALUE_3);

	static uint8_t mmio_cmpr_4[MMIO_SIZE] = {0,0,0,0,0b00011011,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_4, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Area>() != Test_mmio::Reg::Area::VALUE_3)
	{ return test_failed(4); }

	/**
	 * Test 5, read/write bitarea overflowing
	 */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::Reg::Area>(0b11111101);

	static uint8_t mmio_cmpr_5[MMIO_SIZE] = {0,0,0,0,0b00001010,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_5, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Area>() != 0b101)
	{ return test_failed(5); }

	/**
	 * Test 6, read/write bit out of regrange
	 */
	mmio.write<Test_mmio::Reg::Invalid_bit>(1);

	if (compare_mem(mmio_mem, mmio_cmpr_5, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Invalid_bit>() != 0)
	{ return test_failed(6); }

	/**
	 * Test 7, read/write bitarea that exceeds regrange
	 */
	mmio.write<Test_mmio::Reg::Invalid_area>(0xff);

	static uint8_t mmio_cmpr_7[MMIO_SIZE] = {0,0,0,0,0b11001010,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_7, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Invalid_area>() != 0b11)
	{ return test_failed(7); }

	/**
	 * Test 8, read/write bitarea that overlaps other subregs
	 */
	mmio.write<Test_mmio::Reg::Overlapping_area>(0b00110011);

	static uint8_t mmio_cmpr_8[MMIO_SIZE] = {0,0,0,0,0b11110011,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_8, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Overlapping_area>() != 0b110011)
	{ return test_failed(8); }


	/****************************
	 ** Genode::Register tests **
	 ****************************/

	/**
	 * Test 9, read/write subregs appropriately, overflowing and out of range
	 */
	Cpu_state::storage_t state = Cpu_state::read();
	Cpu_state::Mode::set(state, Cpu_state::Mode::MONITOR);
	Cpu_state::A::set(state, 1);
	Cpu_state::B::set(state);
	Cpu_state::C::set(state, 0xdddd);
	Cpu_state::Irq::set(state, 0xdddd);
	Cpu_state::Invalid_bit::set(state, 0xdddd);
	Cpu_state::Invalid_area::set(state, 0xdddd);
	Cpu_state::write(state);

	state = Cpu_state::read();
	if (cpu_state != 0b1101010101001010
	    || Cpu_state::Mode::get(state)         != Cpu_state::Mode::MONITOR
	    || Cpu_state::A::get(state)            != 1
	    || Cpu_state::B::get(state)            != 1
	    || Cpu_state::C::get(state)            != 1
	    || Cpu_state::Irq::get(state)          != 0b101
	    || Cpu_state::Invalid_bit::get(state)  != 0
	    || Cpu_state::Invalid_area::get(state) != 1)
	{ return test_failed(9); }

	/**
	 * Test 10, clear subregs
	 */
	Cpu_state::B::clear(state);
	Cpu_state::Irq::clear(state);
	Cpu_state::write(state);

	state = Cpu_state::read();
	if (cpu_state != 0b1000010001001010
	    || Cpu_state::B::get(state)   != 0
	    || Cpu_state::Irq::get(state) != 0)
	{ return test_failed(10); }

	/**
	 * Test 11, read/write register array items with array- and item overflows 
	 */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::Array>(0xa,  0);
	mmio.write<Test_mmio::Array>(0xb,  4);
	mmio.write<Test_mmio::Array>(0xc,  5);
	mmio.write<Test_mmio::Array>(0xdd, 9);
	mmio.write<Test_mmio::Array>(0xff, 11);

	static uint8_t mmio_cmpr_11[MMIO_SIZE] = {0,0,0x0a,0,0xcb,0,0xd0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_11, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Array>(0) != 0xa ||
	    mmio.read<Test_mmio::Array>(4) != 0xb ||
	    mmio.read<Test_mmio::Array>(5) != 0xc ||
	    mmio.read<Test_mmio::Array>(9) != 0xd ||
	    mmio.read<Test_mmio::Array>(11) !=  0 )
	{ return test_failed(11); }

	/**
	 * Test 12, read/write subregs of register array items with array-, item- and subreg overflows
	 *          also test overlappng subregs
	 */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::Array::A>(0x1, 0);
	mmio.write<Test_mmio::Array::B>(0x2, 0);
	mmio.write<Test_mmio::Array::A>(0x1, 1);
	mmio.write<Test_mmio::Array::B>(0x1, 1);
	mmio.write<Test_mmio::Array::A>(0xf, 4);
	mmio.write<Test_mmio::Array::B>(0xe, 4);
	mmio.write<Test_mmio::Array::D>(0xd, 5);
	mmio.write<Test_mmio::Array::C>(0x1, 8);
	mmio.write<Test_mmio::Array::D>(0x3, 8);
	mmio.write<Test_mmio::Array::A>(0xf, 11);

	static uint8_t mmio_cmpr_12[MMIO_SIZE] = {0,0,0b00110101,0,0b10100101,0,0b00000110,0};
	if (compare_mem(mmio_mem, mmio_cmpr_12, sizeof(mmio_mem)) ||
		mmio.read<Test_mmio::Array::A>(0)  != 0x1 ||
		mmio.read<Test_mmio::Array::B>(0)  != 0x2 ||
		mmio.read<Test_mmio::Array::A>(1)  != 0x1 ||
		mmio.read<Test_mmio::Array::B>(1)  != 0x1 ||
		mmio.read<Test_mmio::Array::A>(4)  != 0x1 ||
		mmio.read<Test_mmio::Array::B>(4)  != 0x2 ||
		mmio.read<Test_mmio::Array::D>(5)  != 0x5 ||
		mmio.read<Test_mmio::Array::C>(8)  != 0x0 ||
		mmio.read<Test_mmio::Array::D>(8)  != 0x3 ||
		mmio.read<Test_mmio::Array::A>(11) != 0   )
	{ return test_failed(12); }

	printf("Test ended successfully\n");
	return 0;
}


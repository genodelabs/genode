/*
 * \brief  Diversified test of the Register and MMIO framework
 * \author Martin Stein
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <util/mmio.h>

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
struct Cpu_state : Register<16>
{
	struct Mode : Bitfield<0,4>
	{
		enum {
			KERNEL  = 0b1000,
			USER    = 0b1001,
			MONITOR = 0b1010,
		};
	};
	struct A   : Bitfield<6,1> { };
	struct B   : Bitfield<8,1> { };
	struct C   : Bitfield<10,1> { };
	struct Irq : Bitfield<12,3> { };

	struct Invalid_bit  : Bitfield<18,1> { };
	struct Invalid_area : Bitfield<15,4> { };

	inline static access_t read() { return cpu_state; }

	inline static void write(access_t & v) { cpu_state = v; }
};

/**
 * Exemplary MMIO region type
 */
struct Test_mmio : public Mmio
{
	Test_mmio(addr_t const base) : Mmio(base) { }

	struct Reg_64 : Register<0x00, 64>
	{
		struct Bits_0 : Bitfield<48,12> { };
		struct Bits_1 : Bitfield<24,20> { };
		struct Bits_2 : Bitfield<44,4> { };
		struct Bits_3 : Bitfield<0,24> { };
		struct Bits_4 : Bitfield<60,4> { };
		struct Bits_5 : Bitfield<0,64> { };
		struct Bits_6 : Bitfield<16,64> { };
		struct Bits_7 : Bitfield<12,64> { };
		struct Bits_8 : Bitfield<0,64> { };
	};
	struct Bitset_64_0 : Bitset_2<Reg_64::Bits_0, Reg_64::Bits_1> { };
	struct Bitset_64_1 : Bitset_3<Reg_64::Bits_4, Reg_64::Bits_3, Reg_64::Bits_2> { };
	struct Bitset_64   : Bitset_2<Bitset_64_0, Bitset_64_1> { };

	struct Reg : Register<0x04, 8>
	{
		enum
		{
			/* ensure that we can not falsely overlay inherited enums */
			OFFSET = 0x1234,
			ACCESS_WIDTH = 1,
			STRICT_WRITE = 1,
		};

		struct Bit_1 : Bitfield<0,1> { };
		struct Area  : Bitfield<1,3>
		{
			enum {
				VALUE_1 = 3,
				VALUE_2 = 4,
				VALUE_3 = 5,
			};
		};
		struct Bit_2            : Bitfield<4,1> { };
		struct Invalid_bit      : Bitfield<8,1> { };
		struct Invalid_area     : Bitfield<6,8> { };
		struct Overlapping_area : Bitfield<0,6> { };
	};

	struct Array : Register_array<0x2, 16, 10, 4>
	{
		enum
		{
			/* ensure that we can not falsely overlay inherited enums */
			STRICT_WRITE    = 1,
			OFFSET          = 0x1234,
			ACCESS_WIDTH    = 1,
			ITEMS           = 1,
			ITEM_WIDTH      = 1,
		};

		struct A : Bitfield<0,1> { };
		struct B : Bitfield<1,2> { };
		struct C : Bitfield<3,1> { };
		struct D : Bitfield<1,3> { };
	};

	struct Strict_array : Register_array<0x0, 16, 10, 4, true>
	{
		struct A : Bitfield<1,1> { };
		struct B : Bitfield<2,4> { };
	};

	struct Simple_array_1 : Register_array<0x0, 32, 2, 32> { };

	struct Simple_array_2 : Register_array<0x2, 16, 4, 16> { };

	struct Strict_reg : Register<0x0, 32, true>
	{
		struct A : Bitfield<3,2> { };
		struct B : Bitfield<30,4> { };
	};

	struct Reg_0 : Register<0x1, 8> { };

	struct Reg_1 : Register<0x2, 16>
	{
		struct Bits_0 : Bitfield<1, 3> { };
		struct Bits_1 : Bitfield<12, 4> { };
		struct Bits_2 : Bitfield<6, 2> { };
	};
	struct Reg_2 : Register<0x4, 32>
	{
		struct Bits_0 : Bitfield<4, 5> { };
		struct Bits_1 : Bitfield<17, 12> { };
	};
	struct My_bitset_2 : Bitset_2<Reg_1::Bits_0, Reg_0> { };
	struct My_bitset_3 : Bitset_3<Reg_0, Reg_2::Bits_1, Reg_2::Bits_0> { };
	struct My_bitset_4 : Bitset_2<My_bitset_2, Reg_2::Bits_0> { };
	struct My_bitset_5 : Bitset_3<Reg_1::Bits_2, Reg_1::Bits_0, Reg_1::Bits_1> { };
};


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
void failed(unsigned  line,
            Env      &env)
{
	Genode::error("Test in line ", line, " failed");
	env.parent().exit(-1);
}


void Component::construct(Genode::Env &env)
{
	using ::Cpu_state;

	/************************************
	 ** 'Genode::Mmio::Register' tests **
	 ************************************/

	/* use 'Bitfield::bits' with overflowing values */
	Test_mmio mmio((addr_t)&mmio_mem[0]);
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::Reg>(
		Test_mmio::Reg::Bit_1::bits(7) |
		Test_mmio::Reg::Area::bits(10) |
		Test_mmio::Reg::Bit_2::bits(9) );

	static uint8_t mmio_cmpr_1[MMIO_SIZE] = {0,0,0,0,0b00010101,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_1, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg>() != 0x15)
	{ failed(__LINE__, env); }

	/* read/write bit appropriately */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::Reg::Bit_1>(1);
	static uint8_t mmio_cmpr_2[MMIO_SIZE] = {0,0,0,0,0b00000001,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_2, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Bit_1>() != 1)
	{ failed(__LINE__, env); }

	/* read/write bit overflowing */
	mmio.write<Test_mmio::Reg::Bit_2>(0xff);

	static uint8_t mmio_cmpr_3[MMIO_SIZE] = {0,0,0,0,0b00010001,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_3, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Bit_2>() != 1)
	{ failed(__LINE__, env); }

	/* read/write bitarea appropriately */
	mmio.write<Test_mmio::Reg::Area>(Test_mmio::Reg::Area::VALUE_3);
	static uint8_t mmio_cmpr_4[MMIO_SIZE] = {0,0,0,0,0b00011011,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_4, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Area>() != Test_mmio::Reg::Area::VALUE_3)
	{ failed(__LINE__, env); }

	/* read/write bitarea overflowing */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::Reg::Area>(0b11111101);
	static uint8_t mmio_cmpr_5[MMIO_SIZE] = {0,0,0,0,0b00001010,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_5, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Area>() != 0b101)
	{ failed(__LINE__, env); }

	/* read/write bit out of regrange */
	mmio.write<Test_mmio::Reg::Invalid_bit>(1);
	if (compare_mem(mmio_mem, mmio_cmpr_5, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Invalid_bit>() != 0)
	{ failed(__LINE__, env); }

	/* read/write bitarea that exceeds regrange */
	mmio.write<Test_mmio::Reg::Invalid_area>(0xff);
	static uint8_t mmio_cmpr_7[MMIO_SIZE] = {0,0,0,0,0b11001010,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_7, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Invalid_area>() != 0b11)
	{ failed(__LINE__, env); }

	/* read/write bitarea that overlaps other bitfields */
	mmio.write<Test_mmio::Reg::Overlapping_area>(0b00110011);
	static uint8_t mmio_cmpr_8[MMIO_SIZE] = {0,0,0,0,0b11110011,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_8, sizeof(mmio_mem)) ||
	    mmio.read<Test_mmio::Reg::Overlapping_area>() != 0b110011)
	{ failed(__LINE__, env); }


	/******************************
	 ** 'Genode::Register' tests **
	 ******************************/

	/* overflowing and out of range */
	Cpu_state::access_t state = Cpu_state::read();
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
	{ failed(__LINE__, env); }

	/* clear bitfields */
	Cpu_state::B::clear(state);
	Cpu_state::Irq::clear(state);
	Cpu_state::write(state);
	state = Cpu_state::read();
	if (cpu_state != 0b1000010001001010
	    || Cpu_state::B::get(state)   != 0
	    || Cpu_state::Irq::get(state) != 0)
	{ failed(__LINE__, env); }


	/******************************************
	 ** 'Genode::Mmio::Register_array' tests **
	 ******************************************/

	/* read/write register array items with array- and item overflows */
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
	{ failed(__LINE__, env); }

	/* item- and bitfield overflows - also test overlapping bitfields */
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
	{ failed(__LINE__, env); }

	/* writing to registers with 'STRICT_WRITE' set */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	*(uint8_t*)((addr_t)mmio_mem + sizeof(uint32_t)) = 0xaa;
	mmio.write<Test_mmio::Strict_reg::A>(0xff);
	mmio.write<Test_mmio::Strict_reg::B>(0xff);
	static uint8_t mmio_cmpr_13[MMIO_SIZE] = {0,0,0,0b11000000,0b10101010,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_13, sizeof(mmio_mem))) {
		failed(__LINE__, env); }

	/* writing to register array items with 'STRICT_WRITE' set */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	*(uint8_t*)((addr_t)mmio_mem + sizeof(uint16_t)) = 0xaa;
	mmio.write<Test_mmio::Strict_array>(0b1010, 0);
	mmio.write<Test_mmio::Strict_array>(0b1010, 1);
	mmio.write<Test_mmio::Strict_array>(0b1010, 2);
	mmio.write<Test_mmio::Strict_array>(0b1100, 3);
	mmio.write<Test_mmio::Strict_array>(0b110011, 3);
	static uint8_t mmio_cmpr_14[MMIO_SIZE] = {0,0b00110000,0b10101010,0,0,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_14, sizeof(mmio_mem))) {
		failed(__LINE__, env); }

	/* writing to register array bitfields with 'STRICT_WRITE' set */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	*(uint8_t*)((addr_t)mmio_mem + sizeof(uint16_t)) = 0xaa;
	mmio.write<Test_mmio::Strict_array::A>(0xff, 3);
	mmio.write<Test_mmio::Strict_array::B>(0xff, 3);
	static uint8_t mmio_cmpr_15[MMIO_SIZE] = {0,0b11000000,0b10101010,0,0,0,0,0};
	if (compare_mem(mmio_mem, mmio_cmpr_15, sizeof(mmio_mem))) {
		failed(__LINE__, env); }

	/* writing to simple register array */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	*(uint8_t*)((addr_t)mmio_mem + sizeof(uint16_t)) = 0xaa;
	mmio.write<Test_mmio::Simple_array_1>(0x12345678, 0);
	mmio.write<Test_mmio::Simple_array_1>(0x87654321, 1);

	mmio.write<Test_mmio::Simple_array_2>(0xfedc, 0);
	mmio.write<Test_mmio::Simple_array_2>(0xabcd, 2);
	static uint8_t mmio_cmpr_16[MMIO_SIZE] = {0x78,0x56,0xdc,0xfe,0x21,0x43,0xcd,0xab};
	if (compare_mem(mmio_mem, mmio_cmpr_16, sizeof(mmio_mem))) {
		failed(__LINE__, env); }

	/* write and read a bitset with 2 parts */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::My_bitset_2>(0x1234);
	static uint8_t mmio_cmpr_17[MMIO_SIZE] = {0x00,0x46,0x08,0x00,0x00,0x00,0x00,0x00};
	if (compare_mem(mmio_mem, mmio_cmpr_17, sizeof(mmio_mem)))
		failed(__LINE__, env);
	if (mmio.read<Test_mmio::My_bitset_2>() != 0x234)
		failed(__LINE__, env);

	/* write and read a bitset with 3 parts */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::My_bitset_3>(0x12345678);
	static uint8_t mmio_cmpr_18[MMIO_SIZE] = {0x00,0x78,0x00,0x00,0x30,0x00,0xac,0x08};
	if (compare_mem(mmio_mem, mmio_cmpr_18, sizeof(mmio_mem)))
		failed(__LINE__, env);
	if (mmio.read<Test_mmio::My_bitset_3>() != 0x345678)
		failed(__LINE__, env);

	/* write and read a nested bitset */
	zero_mem(mmio_mem, sizeof(mmio_mem));
	mmio.write<Test_mmio::My_bitset_4>(0x5679);
	static uint8_t mmio_cmpr_19[MMIO_SIZE] = {0x00,0xcf,0x02,0x00,0xa0,0x00,0x00,0x00};
	if (compare_mem(mmio_mem, mmio_cmpr_19, sizeof(mmio_mem)))
		failed(__LINE__, env);
	if (mmio.read<Test_mmio::My_bitset_4>() != 0x5679)
		failed(__LINE__, env);

	/* bitfield methods at bitsets */
	Test_mmio::Reg_1::access_t bs5 =
		Test_mmio::My_bitset_5::bits(0b1011110010110);
	if (bs5 != 0b1100000010001010) {
		failed(__LINE__, env);
	}
	if (Test_mmio::My_bitset_5::get(bs5) != 0b110010110) {
		failed(__LINE__, env);
	}
	Test_mmio::My_bitset_5::set(bs5, 0b1011101101001);
	if (bs5 != 0b1011000001000100) {
		failed(__LINE__, env);
	}

	/**********************************
	 ** Test access widths of 64 bit **
	 **********************************/
	{
		/* whole register */
		typedef Test_mmio::Reg_64 Reg;
		enum { REG = 0x0123456789abcdef };
		static uint8_t cmp_mem[MMIO_SIZE] = {
			0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01 };
		zero_mem(mmio_mem, MMIO_SIZE);
		mmio.write<Reg>(REG);
		if (mmio.read<Reg>() != REG) { failed(__LINE__, env); }
		if (compare_mem(mmio_mem, cmp_mem, MMIO_SIZE)) { failed(__LINE__, env); }

		/* bitfields in a register */
		enum {
			BITS_0     = 0x123,
			BITS_1     = 0x56789,
			BITS_2     = 0x4,
			BITS_3     = 0xabcdef,
			BITS_TRASH = 0xf000000,
		};
		zero_mem(mmio_mem, MMIO_SIZE);
		mmio.write<Reg::Bits_0>(BITS_0 | BITS_TRASH);
		mmio.write<Reg::Bits_1>(BITS_1 | BITS_TRASH);
		mmio.write<Reg::Bits_2>(BITS_2 | BITS_TRASH);
		mmio.write<Reg::Bits_3>(BITS_3 | BITS_TRASH);
		if (mmio.read<Reg::Bits_0>() != BITS_0) { failed(__LINE__, env); }
		if (mmio.read<Reg::Bits_1>() != BITS_1) { failed(__LINE__, env); }
		if (mmio.read<Reg::Bits_2>() != BITS_2) { failed(__LINE__, env); }
		if (mmio.read<Reg::Bits_3>() != BITS_3) { failed(__LINE__, env); }
		if (compare_mem(mmio_mem, cmp_mem, MMIO_SIZE)) { failed(__LINE__, env); }

		/* test bitfields that are at least as wide as the register */
		{
			uint64_t const value_1 { 0x0123456789abcdef };
			uint64_t const value_2 { 0x0123456789abcdef };
			uint64_t const value_3 { 0x0123456789abcdef };
			zero_mem(mmio_mem, MMIO_SIZE);
			mmio.write<Reg::Bits_5>(value_1);
			if (mmio.read<Reg::Bits_5>() != value_2) { failed(__LINE__, env); }
			if (compare_mem(mmio_mem, (uint8_t *)&value_3, MMIO_SIZE)) { failed(__LINE__, env); }
		}
		{
			uint64_t const value_1 { 0x0123456789abcdef };
			uint64_t const value_2 { 0x0000456789abcdef };
			uint64_t const value_3 { 0x456789abcdef0000 };
			zero_mem(mmio_mem, MMIO_SIZE);
			mmio.write<Reg::Bits_6>(value_1);
			if (mmio.read<Reg::Bits_6>() != value_2) { failed(__LINE__, env); }
			if (compare_mem(mmio_mem, (uint8_t *)&value_3, MMIO_SIZE)) { failed(__LINE__, env); }
		}
		{
			uint64_t const value_1 { 0x0123456789abcdef };
			uint64_t const value_2 { 0x0003456789abcdef };
			uint64_t const value_3 { 0x3456789abcdef000 };
			zero_mem(mmio_mem, MMIO_SIZE);
			mmio.write<Reg::Bits_7>(value_1);
			if (mmio.read<Reg::Bits_7>() != value_2) { failed(__LINE__, env); }
			if (compare_mem(mmio_mem, (uint8_t *)&value_3, MMIO_SIZE)) { failed(__LINE__, env); }
		}
		{
			uint64_t const value_1 { 0x0123456789abcdef };
			uint64_t const value_2 { 0x0123456789abcdef };
			uint64_t const value_3 { 0x0123456789abcdef };
			zero_mem(mmio_mem, MMIO_SIZE);
			mmio.write<Reg::Bits_8>(value_1);
			if (mmio.read<Reg::Bits_8>() != value_2) { failed(__LINE__, env); }
			if (compare_mem(mmio_mem, (uint8_t *)&value_3, MMIO_SIZE)) { failed(__LINE__, env); }
		}

		/* bitsets */
		typedef Test_mmio::Bitset_64 Bitset;
		enum { BITSET = 0x4abcdef056789123 };
		zero_mem(mmio_mem, MMIO_SIZE);
		mmio.write<Bitset>(BITSET);
		if (mmio.read<Bitset>() != BITSET) { failed(__LINE__, env); }
		if (compare_mem(mmio_mem, cmp_mem, MMIO_SIZE)) { failed(__LINE__, env); }
	}
	env.parent().exit(0);
}


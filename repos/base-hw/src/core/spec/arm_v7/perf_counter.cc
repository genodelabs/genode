/*
 * \brief  Performance counter ARMv7
 * \author Josef Soentgen
 * \date   2013-09-26
 *
 * The naming is based on ARM Architecture Reference Manual ARMv7-A.
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/register.h>

/* base-hw includes */
#include <kernel/perf_counter.h>

using namespace Genode;

/**
 * Performance Monitor Control Register
 */
struct Pmcr : Register<32>
{
	struct E : Bitfield<0,1> { }; /* enable all counters */
	struct P : Bitfield<1,1> { }; /* performance counter reset */
	struct C : Bitfield<2,1> { }; /* cycle counter reset */
	struct D : Bitfield<3,1> { }; /* clock divider */

	static access_t enable_and_reset()
	{
		access_t v = 0;
		E::set(v, 1);
		P::set(v, 1);
		C::set(v, 1);
		return v;
	}

	static access_t read()
	{
		access_t v;
		asm volatile("mrc p15, 0, %[v], c9, c12, 0" : [v]"=r"(v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile("mcr p15, 0, %[v], c9, c12, 0" :: [v]"r"(v) : ); }
};


/**
 * Interrupt Enable Clear Register
 */
struct Pmintenclr : Register<32>
{
	struct C  : Bitfield<31,1> { }; /* disable cycle counter overflow IRQ */
	struct P0 : Bitfield<0,1>  { }; /* disable pmc0 overflow IRQ */
	struct P1 : Bitfield<1,1>  { }; /* disable pmc1 overflow IRQ */

	static access_t disable_overflow_intr()
	{
		access_t v = 0;
		C::set(v, 1) ;
		P0::set(v, 1);
		P1::set(v, 1);
		return v;
	}

	static access_t read()
	{
		access_t v;
		asm volatile("mrc p15, 0, %[v], c9, c14, 2" : [v]"=r"(v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile("mcr p15, 0, %[v], c9, c14, 2" :: [v]"r"(v) : ); }
};


/**
 * Count Enable Set Register
 */
struct Pmcntenset : Register<32>
{
	struct C  : Bitfield<31,1> { }; /* cycle counter enable */
	struct P0 : Bitfield<0,1>  { }; /* counter 0 enable */
	struct P1 : Bitfield<1,1>  { }; /* counter 1 enable */
	struct P2 : Bitfield<2,1>  { }; /* counter 2 enable */
	struct P3 : Bitfield<3,1>  { }; /* counter 3 enable */

	static access_t enable_counter()
	{
		access_t v = 0;
		C::set(v, 1);
		P0::set(v, 1);
		P1::set(v, 1);
		P2::set(v, 1);
		P3::set(v, 1);
		return v;
	}

	static access_t read()
	{
		access_t v;
		asm volatile("mrc p15, 0, %[v], c9, c12, 1" : [v]"=r"(v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile("mcr p15, 0, %0, c9, c12, 1" :: [v]"r"(v) : ); }
};


/**
 * Overflow Flag Status Register
 */
struct Pmovsr : Register<32>
{
	struct C  : Bitfield<31,1> { }; /* cycle counter overflow flag */
	struct P0 : Bitfield<0,1>  { }; /* counter 0 overflow flag */
	struct P1 : Bitfield<1,1>  { }; /* counter 1 overflow flag */

	static access_t clear_overflow_flags()
	{
		access_t v = 0;
		C::set(v, 1);
		P0::set(v, 1);
		P1::set(v, 1);
		return v;
	}

	static access_t read()
	{
		access_t v;
		asm volatile("mrc p15, 0, %[v], c9, c12, 3" : [v]"=r"(v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile("mcr p15, 0, %0, c9, c12, 3" :: [v]"r"(v) : ); }
};


/**
 * User Enable Register
 */
struct Pmuseren : Register<32>
{
	struct En : Bitfield<0,1> { }; /* enable user mode access */

	static access_t enable() { return En::bits(1); }

	static access_t read()
	{
		access_t v;
		asm volatile("mrc p15, 0, %[v], c9, c14, 0" : [v]"=r"(v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile("mcr p15, 0, %0, c9, c14, 0" :: [v]"r"(v) : ); }
};


void Kernel::Perf_counter::enable()
{
	/* program PMU and enable all counters */
	Pmcr::write(Pmcr::enable_and_reset());
	Pmcntenset::write(Pmcntenset::enable_counter());
	Pmovsr::write(Pmovsr::clear_overflow_flags());

	/* enable user-mode access to counters and disable overflow interrupt. */
	Pmuseren::write(Pmuseren::enable());
	Pmintenclr::write(Pmintenclr::disable_overflow_intr());
}


Kernel::Perf_counter* Kernel::perf_counter()
{
	static Kernel::Perf_counter inst;
	return &inst;
}

/*
 * \brief  Performance counter for ARMv6
 * \author Josef Soentgen
 * \date   2013-09-26
 *
 * The naming is based on ARM1176JZF-S Technical Reference Manual.
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
	struct E : Bitfield<0,1> { }; /* enable all counter */
	struct P : Bitfield<1,1> { }; /* count register reset */
	struct C : Bitfield<2,1> { }; /* cycle counter reset */
	struct D : Bitfield<3,1> { }; /* cycle counter divider */

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
		asm volatile("mrc p15, 0, %[v], c15, c12, 0" : [v]"=r"(v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile("mcr p15, 0, %[v], c15, c12, 0" :: [v]"r"(v) : ); }
};


/**
 * System Validation Counter Register
 */
struct Sysvalcntrr : Register<32>
{
	static access_t reset_counter() { return 0; }

	static access_t read()
	{
		access_t v;
		asm volatile("mrc p15, 0, %[v], c15, c12, 1" : [v]"=r"(v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile("mcr p15, 0, %[v], c15, c12, 1" :: [v]"r"(v) : ); }
};


/**
 * Secure User and Non-secure Access Validation Control Register
 */
struct Accvalctlr : Register<32>
{
	struct V : Bitfield<0,1> { }; /* enable access in user-mode */

	static access_t enable_user_access() { return V::bits(1); }

	static access_t read()
	{
		access_t v;
		asm volatile("mrc p15, 0, %[v], c15, c9, 0" : [v]"=r"(v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile("mcr p15, 0, %[v], c15, c9, 0" :: [v]"r"(v) : ); }
};


void Kernel::Perf_counter::enable()
{
	/* enable counters and disable overflow interrupt. */
	Pmcr::access_t v = Pmcr::enable_and_reset();
	Pmcr::D::set(v, 1); /* count every 64 cycles */
	Pmcr::write(v);

	Sysvalcntrr::write(Sysvalcntrr::reset_counter());

	/* enable user-mode access */
	Accvalctlr::write(Accvalctlr::enable_user_access());
}


Kernel::Perf_counter* Kernel::perf_counter()
{
	static Kernel::Perf_counter inst;
	return &inst;
}

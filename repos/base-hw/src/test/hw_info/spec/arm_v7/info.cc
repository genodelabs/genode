/*
 * \brief  Provide detailed hardware Information
 * \author Martin Stein
 * \date   2014-10-20
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base includes */
#include <util/register.h>
#include <base/printf.h>

using namespace Genode;

struct Reg_32_8: Register<32>
{
	struct B0 : Bitfield<0,4> { };
	struct B1 : Bitfield<4,4> { };
	struct B2 : Bitfield<8,4> { };
	struct B3 : Bitfield<12,4> { };
	struct B4 : Bitfield<16,4> { };
	struct B5 : Bitfield<20,4> { };
	struct B6 : Bitfield<24,4> { };
	struct B7 : Bitfield<28,4> { };
};

struct Id_pfr1 : Reg_32_8
{
	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c1, 1" : "=r" (v) :: );
		return v;
	}
};

struct Id_mmfr0 : Reg_32_8
{
	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c1, 4" : "=r" (v) :: );
		return v;
	}
};

struct Id_mmfr1 : Reg_32_8
{
	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c1, 5" : "=r" (v) :: );
		return v;
	}
};

struct Id_mmfr2 : Reg_32_8
{
	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c1, 6" : "=r" (v) :: );
		return v;
	}
};

struct Id_mmfr3 : Reg_32_8
{
	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c1, 7" : "=r" (v) :: );
		return v;
	}
};

struct Id_pfr0 : Reg_32_8
{
	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c1, 0" : "=r" (v) :: );
		return v;
	}
};

struct Ctr : Register<32>
{
	struct Iminline : Bitfield<0,4> { };
	struct L1ip     : Bitfield<14,2> { };
	struct Dminline : Bitfield<16,4> { };
	struct Erg      : Bitfield<20,4> { };
	struct Cgw      : Bitfield<24,4> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c0, 1" : "=r" (v) :: );
		return v;
	}
};

struct Ccsidr : Register<32>
{
	struct Line_size     : Bitfield<0,3> { };
	struct Associativity : Bitfield<3,10> { };
	struct Num_sets      : Bitfield<13,15> { };
	struct Wa            : Bitfield<28,1> { };
	struct Ra            : Bitfield<29,1> { };
	struct Wb            : Bitfield<30,1> { };
	struct Wt            : Bitfield<31,1> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (v) :: );
		return v;
	}
};

struct Clidr : Register<32>
{
	struct Ctype1 : Bitfield<0,3> { };
	struct Ctype2 : Bitfield<3,3> { };
	struct Ctype3 : Bitfield<6,3> { };
	struct Ctype4 : Bitfield<9,3> { };
	struct Ctype5 : Bitfield<12,3> { };
	struct Ctype6 : Bitfield<15,3> { };
	struct Ctype7 : Bitfield<18,3> { };
	struct Louis  : Bitfield<21,3> { };
	struct Loc    : Bitfield<24,3> { };
	struct Louu   : Bitfield<27,3> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 1, %0, c0, c0, 1" : "=r" (v) :: );
		return v;
	}
};

struct Fpsid : Register<32>
{
	struct Revision        : Bitfield<0,4> { };
	struct Variant         : Bitfield<4,4> { };
	struct Part_number     : Bitfield<8,8> { };
	struct Subarchitecture : Bitfield<16,7> { };
	struct Sw              : Bitfield<23,1> { };
	struct Implementer     : Bitfield<24,8> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p10, 7, %0, cr0, cr0" : "=r" (v) :: );
		return v;
	}
};

struct Mvfr0 : Reg_32_8
{
	static access_t read()
	{
		access_t v;
		asm volatile("mrc p10, 7, %0, cr7, cr0" : "=r" (v) :: );
		return v;
	}
};

struct Mvfr1 : Reg_32_8
{
	static access_t read()
	{
		access_t v;
		asm volatile("mrc p10, 7, %0, cr6, cr0" : "=r" (v));
		return v;
	}
};

struct Mpidr : Register<32>
{
	struct Mp : Bitfield<31,1> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c0, 5" : "=r" (v) :: );
		return v;
	}
};

struct Tlbtr : Register<32>
{
	struct Nu : Bitfield<0,1> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c0, 3" : "=r" (v) :: );
		return v;
	}
};

struct Midr : Register<32>
{
	struct Revision            : Bitfield<0,4> { };
	struct Primary_part_number : Bitfield<4,12> { };
	struct Architecture        : Bitfield<16,4> { };
	struct Variant             : Bitfield<20,4> { };
	struct Implementer         : Bitfield<24,8> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r" (v) :: );
		return v;
	}
};

struct Csselr : Register<32>
{
	struct Ind   : Bitfield<0,1> { };
	struct Level : Bitfield<1,3> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 2, %0, c0, c0, 0" : "=r" (v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile ("mcr p15, 2, %0, c0, c0, 0" :: "r" (v) : ); }
};


void info_ccsidr()
{
	printf(" Cache Size Identification Register for L%u %s:\n", Csselr::Level::get(Csselr::read()) + 1, Csselr::Ind::get(Csselr::read()) ? "Instruction Cache" : "Data Cache");
	printf("   Line size:        0x%x\n", Ccsidr::Line_size::get(Ccsidr::read()));
	printf("   Associativity:    0x%x\n", Ccsidr::Associativity::get(Ccsidr::read()));
	printf("   Number of Sets:   0x%x\n", Ccsidr::Num_sets::get(Ccsidr::read()));
	printf("   Write-Allocation: 0x%x\n", Ccsidr::Wa::get(Ccsidr::read()));
	printf("   Read-Allocation:  0x%x\n", Ccsidr::Ra::get(Ccsidr::read()));
	printf("   Write-Back:       0x%x\n", Ccsidr::Wb::get(Ccsidr::read()));
	printf("   Write-Through:    0x%x\n", Ccsidr::Wt::get(Ccsidr::read()));
	printf(" \n");
}


void info_ccsidr_level(unsigned const l, Clidr::access_t const t)
{
	Csselr::access_t s = 0;
	Csselr::Level::set(s, l - 1);
	if (t & 1) { Csselr::Ind::set(s, 1); Csselr::write(s); info_ccsidr(); }
	if (t & 2) { Csselr::Ind::set(s, 0); Csselr::write(s); info_ccsidr(); }
}


void info()
{
	/*
	 * Processor
	 */

	printf("------ ARMv7 processor ------\n");
	printf("\n");

	printf(" Main Identification Register:\n");
	printf("   Revision:            %u\n", Midr::Revision::get(Midr::read()));
	printf("   Primary Part number: %u\n", Midr::Primary_part_number::get(Midr::read()));
	printf("   Architecture:        %u\n", Midr::Architecture::get(Midr::read()));
	printf("   Variant:             %u\n", Midr::Variant::get(Midr::read()));
	printf("   Implementer:         %c\n", Midr::Implementer::get(Midr::read()));
	printf(" \n");

	printf(" Multiprocessor Identification Register 0:\n");
	printf("   Multiprocessor: %u\n", Mpidr::Mp::get(Mpidr::read()));
	printf(" \n");

	printf(" Processor feature register 0:\n");
	printf("   ARM instruction set support:     0x%x\n", Id_pfr0::B0::get(Id_pfr0::read()));
	printf("   Thumb instruction set support:   0x%x\n", Id_pfr0::B1::get(Id_pfr0::read()));
	printf("   Jazelle extension support:       0x%x\n", Id_pfr0::B2::get(Id_pfr0::read()));
	printf("   ThumbEE instruction set support: 0x%x\n", Id_pfr0::B3::get(Id_pfr0::read()));
	printf(" \n");

	printf(" Processor feature register 1:\n");
	printf("   Programmers’ model:          0x%x\n", Id_pfr1::B0::get(Id_pfr1::read()));
	printf("   Security Extensions:         0x%x\n", Id_pfr1::B1::get(Id_pfr1::read()));
	printf("   M profile programmers model: 0x%x\n", Id_pfr1::B2::get(Id_pfr1::read()));
	printf("   Virtualization Extensions:   0x%x\n", Id_pfr1::B3::get(Id_pfr1::read()));
	printf("   Generic Timer Extension:     0x%x\n", Id_pfr1::B4::get(Id_pfr1::read()));
	printf(" \n");

	/*
	 * Memory Model
	 */

	printf("------ ARMv7 memory model ------\n");
	printf("\n");

	printf(" Memory model feature register 0:\n");
	printf("   VMSA support:           0x%x\n", Id_mmfr0::B0::get(Id_mmfr0::read()));
	printf("   PMSA support:           0x%x\n", Id_mmfr0::B1::get(Id_mmfr0::read()));
	printf("   Outermost shareability: 0x%x\n", Id_mmfr0::B2::get(Id_mmfr0::read()));
	printf("   Shareability levels:    0x%x\n", Id_mmfr0::B3::get(Id_mmfr0::read()));
	printf("   TCM support:            0x%x\n", Id_mmfr0::B4::get(Id_mmfr0::read()));
	printf("   Auxiliary registers:    0x%x\n", Id_mmfr0::B5::get(Id_mmfr0::read()));
	printf("   FCSE support:           0x%x\n", Id_mmfr0::B6::get(Id_mmfr0::read()));
	printf("   Innermost shareability: 0x%x\n", Id_mmfr0::B7::get(Id_mmfr0::read()));
	printf(" \n");

	printf(" Memory model feature register 1:\n");
	printf("   L1 Harvard cache VA:      0x%x\n", Id_mmfr1::B0::get(Id_mmfr1::read()));
	printf("   L1 unified cache VA:      0x%x\n", Id_mmfr1::B1::get(Id_mmfr1::read()));
	printf("   L1 Harvard cache set/way: 0x%x\n", Id_mmfr1::B2::get(Id_mmfr1::read()));
	printf("   L1 unified cache set/way: 0x%x\n", Id_mmfr1::B3::get(Id_mmfr1::read()));
	printf("   L1 Harvard cache:         0x%x\n", Id_mmfr1::B4::get(Id_mmfr1::read()));
	printf("   L1 unified cache:         0x%x\n", Id_mmfr1::B5::get(Id_mmfr1::read()));
	printf("   L1 cache test and clean:  0x%x\n", Id_mmfr1::B6::get(Id_mmfr1::read()));
	printf("   Branch predictor:         0x%x\n", Id_mmfr1::B7::get(Id_mmfr1::read()));
	printf(" \n");

	printf(" Memory model feature register 2:\n");
	printf("   L1 Harvard fg fetch: 0x%x\n", Id_mmfr2::B0::get(Id_mmfr2::read()));
	printf("   L1 Harvard bg fetch: 0x%x\n", Id_mmfr2::B1::get(Id_mmfr2::read()));
	printf("   L1 Harvard range:    0x%x\n", Id_mmfr2::B2::get(Id_mmfr2::read()));
	printf("   Harvard TLB:         0x%x\n", Id_mmfr2::B3::get(Id_mmfr2::read()));
	printf("   Unified TLB:         0x%x\n", Id_mmfr2::B4::get(Id_mmfr2::read()));
	printf("   Mem barrier:         0x%x\n", Id_mmfr2::B5::get(Id_mmfr2::read()));
	printf("   WFI stall:           0x%x\n", Id_mmfr2::B6::get(Id_mmfr2::read()));
	printf("   HW Access flag:      0x%x\n", Id_mmfr2::B7::get(Id_mmfr2::read()));
	printf(" \n");

	printf(" Memory model feature register 3:\n");
	printf("   Cache maintain MVA:     0x%x\n", Id_mmfr3::B0::get(Id_mmfr3::read()));
	printf("   Cache maintain set/way: 0x%x\n", Id_mmfr3::B1::get(Id_mmfr3::read()));
	printf("   BP maintain:            0x%x\n", Id_mmfr3::B2::get(Id_mmfr3::read()));
	printf("   Maintenance broadcast:  0x%x\n", Id_mmfr3::B3::get(Id_mmfr3::read()));
	printf("   Coherent walk:          0x%x\n", Id_mmfr3::B5::get(Id_mmfr3::read()));
	printf("   Cached memory size:     0x%x\n", Id_mmfr3::B6::get(Id_mmfr3::read()));
	printf("   Supersection support:   0x%x\n", Id_mmfr3::B7::get(Id_mmfr3::read()));
	printf(" \n");

	printf(" TLB Type Register:\n");
	printf("   Unified TLB: %u\n", !Tlbtr::Nu::get(Tlbtr::read()));
	printf(" \n");

	/*
	 * Caches
	 */

	printf("------ ARMv7 caches ------\n");
	printf("\n");

	printf(" Cache Type Register:\n");
	printf("   Instruction Cache Min Line:       0x%x\n", Ctr::Iminline::get(Ctr::read()));
	printf("   Level 1 Instruction Cache Policy: 0x%x\n", Ctr::L1ip::get(Ctr::read()));
	printf("   Data Cache Min Line:              0x%x\n", Ctr::Dminline::get(Ctr::read()));
	printf("   Exclusives Reservation Granule:   0x%x\n", Ctr::Erg::get(Ctr::read()));
	printf("   Cache Write-back Granule:         0x%x\n", Ctr::Cgw::get(Ctr::read()));
	printf(" \n");

	printf(" Cache Level Identification Register:\n");
	printf("   Cache type 1:                         0x%x\n", Clidr::Ctype1::get(Clidr::read()));
	printf("   Cache type 2:                         0x%x\n", Clidr::Ctype2::get(Clidr::read()));
	printf("   Cache type 3:                         0x%x\n", Clidr::Ctype3::get(Clidr::read()));
	printf("   Cache type 4:                         0x%x\n", Clidr::Ctype4::get(Clidr::read()));
	printf("   Cache type 5:                         0x%x\n", Clidr::Ctype5::get(Clidr::read()));
	printf("   Cache type 6:                         0x%x\n", Clidr::Ctype6::get(Clidr::read()));
	printf("   Cache type 7:                         0x%x\n", Clidr::Ctype7::get(Clidr::read()));
	printf("   Level of Unification Inner Shareable: 0x%x\n", Clidr::Louis::get(Clidr::read()));
	printf("   Level of Coherency:                   0x%x\n", Clidr::Loc::get(Clidr::read()));
	printf("   Level of Unification Uniprocessor:    0x%x\n", Clidr::Louu::get(Clidr::read()));
	printf(" \n");

	info_ccsidr_level(1, Clidr::Ctype1::get(Clidr::read()));
	info_ccsidr_level(2, Clidr::Ctype2::get(Clidr::read()));
	info_ccsidr_level(3, Clidr::Ctype3::get(Clidr::read()));
	info_ccsidr_level(4, Clidr::Ctype4::get(Clidr::read()));
	info_ccsidr_level(5, Clidr::Ctype5::get(Clidr::read()));
	info_ccsidr_level(6, Clidr::Ctype6::get(Clidr::read()));
	info_ccsidr_level(7, Clidr::Ctype7::get(Clidr::read()));

	/*
	 * Advanced SIMD and Floating-point Extensions
	 */

	printf("------ ARMv7 advanced SIMD and floating-point extensions ------\n");
	printf("\n");

	printf(" Floating-point System Identification Register:\n");
	printf("   Revision:           %u\n", Fpsid::Revision::get(Fpsid::read()));
	printf("   Variant:            %u\n", Fpsid::Variant::get(Fpsid::read()));
	printf("   Part number:        %u\n", Fpsid::Part_number::get(Fpsid::read()));
	printf("   Subarchitecture:    %u\n", Fpsid::Subarchitecture::get(Fpsid::read()));
	printf("   Software emulation: %u\n", Fpsid::Sw::get(Fpsid::read()));
	printf("   Implementer:        %c\n",   Fpsid::Implementer::get(Fpsid::read()));
	printf(" \n");

	printf(" Media and VFP Feature Register 0:\n");
	printf("   Advanced SIMD registers: 0x%x\n", Mvfr0::B0::get(Mvfr0::read()));
	printf("   Single-precision:        0x%x\n", Mvfr0::B1::get(Mvfr0::read()));
	printf("   Double-precision:        0x%x\n", Mvfr0::B2::get(Mvfr0::read()));
	printf("   VFP exception trapping:  0x%x\n", Mvfr0::B3::get(Mvfr0::read()));
	printf("   Divide:                  0x%x\n", Mvfr0::B4::get(Mvfr0::read()));
	printf("   Square root:             0x%x\n", Mvfr0::B5::get(Mvfr0::read()));
	printf("   Short vectors:           0x%x\n", Mvfr0::B6::get(Mvfr0::read()));
	printf("   VFP rounding modes:      0x%x\n", Mvfr0::B7::get(Mvfr0::read()));
	printf(" \n");

	printf(" Media and VFP Feature Register 1:\n");
	printf("   Flush-to-Zero mode:                 0x%x\n", Mvfr1::B0::get(Mvfr1::read()));
	printf("   Default NaN mode:                   0x%x\n", Mvfr1::B1::get(Mvfr1::read()));
	printf("   Advanced SIMD load/store:           0x%x\n", Mvfr1::B2::get(Mvfr1::read()));
	printf("   Advanced SIMD integer instructions: 0x%x\n", Mvfr1::B3::get(Mvfr1::read()));
	printf("   Advanced SIMD single-precision FP:  0x%x\n", Mvfr1::B4::get(Mvfr1::read()));
	printf("   Advanced SIMD half-precision FP:    0x%x\n", Mvfr1::B5::get(Mvfr1::read()));
	printf("   VFP half-precision FP conversion:   0x%x\n", Mvfr1::B6::get(Mvfr1::read()));
	printf("   Fused multiply accumulate:          0x%x\n", Mvfr1::B7::get(Mvfr1::read()));
	printf(" \n");
}

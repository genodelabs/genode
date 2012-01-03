/*
 * \brief  Implementation of the Microblaze MMU
 * \author Martin Stein
 * \date   2010-11-08
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DEVICES__XILINX_MICROBLAZE_H_
#define _INCLUDE__DEVICES__XILINX_MICROBLAZE_H_

#include <cpu/config.h>
#include <base/printf.h>
#include <util/math.h>
#include <util/array.h>

namespace Xilinx {

	struct Microblaze
	{
		typedef Cpu::uint8_t  Protection_id;
		typedef Cpu::uint32_t Register;

		class Mmu {

			enum Error {
				SUCCESS           =  0,
				INVALID_ENTRY_ID  = -1,
				INVALID_PAGE_SIZE = -2,
			};

			enum {
				VERBOSE              = 0,
				USE_PROTECTION_ZONES = 0,
				UTLB_SIZE            = 64,

				TLBLO_GUARDED_LSHIFT    = 0,
				TLBLO_MEMCOHER_LSHIFT   = 1,
				TLBLO_INHIBCACHE_LSHIFT = 2,
				TLBLO_WTHROUGH_LSHIFT   = 3,
				TLBLO_ZONE_LSHIFT       = 4,
				TLBLO_WRITEABLE_LSHIFT  = 8,
				TLBLO_EXECUTABLE_LSHIFT = 9,
				TLBLO_REALPAGE_LSHIFT   = 10, TLBLO_REALPAGE_MASK=0x3fffff,

				TLBHI_USER_LSHIFT   = 4,
				TLBHI_ENDIAN_LSHIFT = 5,
				TLBHI_VALID_LSHIFT  = 6,
				TLBHI_SIZE_LSHIFT   = 7,  TLBHI_SIZE_MASK = 0x7,
				TLBHI_TAG_LSHIFT    = 10, TLBHI_TAG_MASK  = 0x3fffff,
			};

			public:

				typedef Cpu::uint8_t  Entry_id;

				enum { MAX_ENTRY_ID = UTLB_SIZE - 1, };

				struct Page
				{
					typedef Cpu::uint8_t Size_id;

					enum {
						MAX_SIZE_LOG2   = 24,
						MAX_SIZE_ID     = 7,
						INVALID_SIZE_ID = MAX_SIZE_ID+1,
					};

					/**
					 * Translation between the native size ID's and the real memory size
					 * the page covers
					 */
					static inline unsigned int size_id_to_size_log2(Size_id const & i);
					static inline Size_id      size_log2_to_size_id(unsigned int const & size_log2);
				};

			private:

				/**
				 * Focus further operations on a specific entry
				 */
				inline void _entry(Entry_id & i);

				/**
				 * Read basic informations from a specific TLB entry
				 */
				inline void _entry(Entry_id      & i,
				                   Register      & tlblo,
				                   Register      & tlbhi,
				                   Protection_id & pid);

				/**
				 * Protection zones constrain access to mappings additionally,
				 * disable this feature
				 */
				inline void _disable_protection_zones();

			public:

				/**
				 * Constructor
				 */
				inline Mmu();

				/**
				 * Get some informations about a specific TLB entry
				 */
				inline signed int get_entry(Entry_id & i, Cpu::addr_t & vbase, 
                                            Protection_id & pid, unsigned int & size_log2);
				/**
				 * Get all informations about a specific TLB entry
				 */
				inline signed int get_entry(Entry_id & i, Cpu::addr_t & pb, Cpu::addr_t & vb, 
				                            Protection_id & pid, unsigned int & size_log2, 
				                            bool & writeable, bool & executable);

				/**
				 * Overwrite a specific TLB entry with a new resolution
				 */
				inline signed int set_entry(Entry_id i, Cpu::addr_t const & pb, Cpu::addr_t const & vb,
                                            Protection_id const & pid, unsigned int const & size_log2, 
                                            bool const & writeable, bool const & executable);

				/**
				 * Render a specific TLB entry ineffective
				 */
				inline void clear_entry(Entry_id i);

				/**
				 * Maximum available entry ID
				 */
				static inline Entry_id max_entry_id() { return (Entry_id) MAX_ENTRY_ID; };
		};

		/**
		 * Read the current stack-pointer 
		 */
		ALWAYS_INLINE static inline Cpu::addr_t stack_pointer();

		/**
		 * Read, write and exchange the current protection ID
		 */
		static inline Protection_id protection_id();
		static inline void          protection_id(Protection_id i);
		static inline void          protection_id(Protection_id & o, Protection_id n);

		inline Mmu * mmu();
	};
}


/**************************************
 * Xilinx::Microblaze implementations *
 **************************************/

Cpu::addr_t Xilinx::Microblaze::stack_pointer()
{
	Register sp;
	asm volatile("add %[sp], r1, r0" :[sp]"=r"(sp)::);
	return (Cpu::addr_t)sp;
}


Xilinx::Microblaze::Mmu * Xilinx::Microblaze::mmu() {
	static Mmu _mmu;
	return &_mmu;
}


Xilinx::Microblaze::Protection_id Xilinx::Microblaze::protection_id()
{
	Protection_id i;
	asm volatile ("mfs %[i], rpid" 
	              : [i] "=r" (i) ::);
	return i;
}


void Xilinx::Microblaze::protection_id(Protection_id i)
{
	asm volatile ("mts rpid, %[i] \n"
	              "bri 4"
	              : [i] "+r" (i) ::);
}


void Xilinx::Microblaze::protection_id(Protection_id & o, Protection_id n)
{
	asm volatile ("mfs %[o], rpid \n"
	              "mts rpid, %[n] \n"
	              "bri 4"
	              : [o] "=r" (o),
	                [n] "+r" (n) ::);
}


/*******************************************
 * Xilinx::Microblaze::Mmu implementations *
 *******************************************/

Xilinx::Microblaze::Mmu::Mmu()
{
	if (!USE_PROTECTION_ZONES) { _disable_protection_zones(); } 
	else { PERR("Protection zones not supported"); }
}


void Xilinx::Microblaze::Mmu::_disable_protection_zones()
{
	asm volatile ("addik r31, r0, 0xC0000000 \n"
	              "mts rzpr, r31 \n"
	              "bri 4"
	              ::: "r31" );
}


signed int Xilinx::Microblaze::Mmu::get_entry(Entry_id & i, Cpu::addr_t & vb, 
                                              Protection_id & pid, unsigned int & size_log2)
{
	if(i>MAX_ENTRY_ID) { return INVALID_ENTRY_ID; };

	Protection_id opid = protection_id();

	/* Read TLB entry */
	asm volatile ("mts rtlbx, %[i]    \n"
	              "bri 4              \n"
	              "mfs %[vb],  rtlbhi \n"
	              "mfs %[pid], rpid"
	              : [i]   "+r" (i),
	                [vb]  "=r" (vb),
	                [pid] "=r" (pid) ::);

	protection_id(opid);

	/**
	 * Decode informations
	 */
	Page::Size_id const s = (vb & (TLBHI_SIZE_MASK<<TLBHI_SIZE_LSHIFT))>>TLBHI_SIZE_LSHIFT;
	size_log2             = Page::size_id_to_size_log2(s);

	vb = Math::round_down<Cpu::addr_t>(vb, size_log2);
	return SUCCESS;
}


signed int Xilinx::Microblaze::Mmu::get_entry(Entry_id & i, Cpu::addr_t & pb, Cpu::addr_t & vb, 
                                              Protection_id & pid, unsigned int & size_log2, 
                                              bool & writeable, bool & executable)
{
	if(i>MAX_ENTRY_ID) { return INVALID_ENTRY_ID; };

	Protection_id opid = protection_id();

	/* Read TLB entry */
	asm volatile ("mts rtlbx, %[i]    \n"
	              "bri 4              \n"
	              "mfs %[pb],  rtlblo \n"
	              "mfs %[vb],  rtlbhi \n"
	              "mfs %[pid], rpid"
	              : [i]   "+r" (i),
	                [pb]  "=r" (pb),
	                [vb]  "=r" (vb),
	                [pid] "=r" (pid) ::);

	protection_id(opid);

	/**
	 * Decode informations
	 */
	writeable  = pb & (1<<TLBLO_WRITEABLE_LSHIFT);
	executable = pb & (1<<TLBLO_EXECUTABLE_LSHIFT);

	Page::Size_id const s = (vb & (TLBHI_SIZE_MASK<<TLBHI_SIZE_LSHIFT))>>TLBHI_SIZE_LSHIFT;
	size_log2             = Page::size_id_to_size_log2(s);

	pb = Math::round_down<Cpu::addr_t>(pb, size_log2);
	vb = Math::round_down<Cpu::addr_t>(vb, size_log2);
	return SUCCESS;
}


signed int Xilinx::Microblaze::Mmu::set_entry(Entry_id i, Cpu::addr_t const & pb, Cpu::addr_t const & vb,
                                              Protection_id const & pid, unsigned int const & size_log2, 
                                              bool const & writeable, bool const & executable)
{
	Protection_id opid;
	protection_id(opid, pid);

	/**
	 * Create TLBLO register value
	 */
	Register tlblo = (Register)Math::round_down<Cpu::addr_t>(pb, size_log2);
	tlblo |= writeable << TLBLO_WRITEABLE_LSHIFT;
	tlblo |= executable << TLBLO_EXECUTABLE_LSHIFT;

	/**
	 * Create TLBHI register value
	 */
	Register tlbhi = Math::round_down<Cpu::addr_t>(vb, size_log2);
	tlbhi |= 1 << TLBHI_VALID_LSHIFT;

	Page::Size_id s = Page::size_log2_to_size_id(size_log2);
	if(s == Page::INVALID_SIZE_ID) { return INVALID_PAGE_SIZE; }
	tlbhi |= ((s & TLBHI_SIZE_MASK) << TLBHI_SIZE_LSHIFT);

	/* Write TLB entry */
	asm volatile ("mts rtlbx, %[i]      \n"
	              "bri 4                \n"
	              "mts rtlblo, %[tlblo] \n"
	              "bri 4                \n"
	              "mts rtlbhi, %[tlbhi] \n"
	              "bri 4"
	              : [i]     "+r" (i),
	                [tlblo] "+r" (tlblo),
	                [tlbhi] "+r" (tlbhi) ::);

	if(VERBOSE) 
	{
		PINF("TLB + %2u[0x%8X..0x%8X) r%c%c\n"
		     "        [0x%8X..0x%8X) 2**%i\n",
		     pid, Math::round_down<Cpu::uint32_t>(vb, size_log2),
		     Math::round_down<Cpu::uint32_t>(vb+(1<<size_log2), size_log2),
		     executable ? 'x' : '-', writeable ? 'w' : '-',
		     Math::round_down<Cpu::uint32_t>(pb, size_log2),
		     Math::round_down<Cpu::uint32_t>(pb+(1<<size_log2), size_log2),
		     size_log2);
	}

	protection_id(opid);

	return SUCCESS;
}


void Xilinx::Microblaze::Mmu::clear_entry(Entry_id i)
{
	if(i>MAX_ENTRY_ID) { return; };

	Protection_id pid = protection_id();

	if(VERBOSE) {
		Cpu::addr_t page;
		Protection_id pid;
		unsigned size_log2;

		if(!get_entry(i, page, pid, size_log2)) {
			PINF("TLB - %i[0x%8X..0x%8X] 2**%i", 
			     pid, (Cpu::uint32_t)page, 
			    (Cpu::uint32_t)page+(1<<size_log2), size_log2);
		}
	}

	/* Zero-fill TLB entry */
	asm volatile("mts rtlbx, %[i] \n"
	             "bri 4           \n"
	             "mts rpid,   r0  \n"
	             "bri 4           \n"
	             "mts rtlbhi, r0  \n"
	             "bri 4           \n"
	             "mts rtlblo, r0  \n"
	             "bri 4"
	             : [i] "+r" (i) ::);

	protection_id(pid);
}


/*************************************************
 * Xilinx::Microblaze::Mmu::Page implementations *
 *************************************************/

Xilinx::Microblaze::Mmu::Page::Size_id 
Xilinx::Microblaze::Mmu::Page::size_log2_to_size_id(unsigned int const & size_log2)
{
	static signed int const _size_log2_to_size_id [MAX_SIZE_LOG2+1] =
		{  INVALID_SIZE_ID, INVALID_SIZE_ID, INVALID_SIZE_ID, INVALID_SIZE_ID, INVALID_SIZE_ID, 
		   INVALID_SIZE_ID, INVALID_SIZE_ID, INVALID_SIZE_ID, INVALID_SIZE_ID, INVALID_SIZE_ID,
		   0, INVALID_SIZE_ID, 1, INVALID_SIZE_ID, 
		   2, INVALID_SIZE_ID, 3, INVALID_SIZE_ID, 
		   4, INVALID_SIZE_ID, 5, INVALID_SIZE_ID, 
		   6, INVALID_SIZE_ID, 7 };

	if(size_log2>MAX_ARRAY_ID(_size_log2_to_size_id)) { return INVALID_SIZE_ID; }
	return (unsigned int)_size_log2_to_size_id[size_log2];
}


unsigned int Xilinx::Microblaze::Mmu::Page::size_id_to_size_log2(Size_id const & i)
{
	static unsigned const _size_id_to_size_log2 [MAX_SIZE_ID+1] =
		{ 10, 12, 14, 16, 18, 20, 22, 24 };

	if(i>ARRAY_SIZE(_size_id_to_size_log2)-1) { return 0; }
	return (unsigned int)_size_id_to_size_log2[i];
}

#endif /* _INCLUDE__DEVICES__XILINX_MICROBLAZE_H_ */


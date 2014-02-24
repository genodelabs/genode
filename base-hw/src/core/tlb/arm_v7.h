/*
 * \brief   TLB driver for core
 * \author  Martin Stein
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TLB__ARM_V7_H_
#define _TLB__ARM_V7_H_

/* core includes */
#include <tlb/arm.h>
#include <cpu/arm_v7.h>

namespace Arm_v7
{
	using namespace Genode;

	/**
	 * ARMv7 first level translation table
	 */
	class Section_table : public Arm::Section_table
	{
		public:

			/**
			 * Link to second level translation-table
			 */
			struct Page_table_descriptor : Arm::Section_table::Page_table_descriptor
			{
				struct Ns : Bitfield<3, 1> { }; /* non-secure bit */

				/**
				 * Compose descriptor value
				 */
				static access_t create(Arm::Page_table * const pt,
				                       Section_table * const st)
				{
					return Arm::Section_table::Page_table_descriptor::create(pt) |
					       Ns::bits(!st->secure());
				}
			};

			/**
			 * Section translation descriptor
			 */
			struct Section : Arm::Section_table::Section
			{
				struct Ns : Bitfield<19, 1> { }; /* non-secure bit */

				/**
				 * Compose descriptor value
				 */
				static access_t create(Page_flags const &flags,
				                       addr_t const pa,
				                       Section_table * const st)
				{
					return Arm::Section_table::Section::create(flags, pa) |
					       Ns::bits(!st->secure());
				}
			};

		protected:

			/* if this table is dedicated to secure mode or to non-secure mode */
			bool _secure;

		public:

			/**
			 * Constructor
			 */
			Section_table() : _secure(Arm_v7::Cpu::secure_mode()) { }

			/**
			 * Insert one atomic translation into this table
			 *
			 * For details see 'Arm::Section_table::insert_translation'
			 */
			size_t
			insert_translation(addr_t const vo, addr_t const pa,
			                   size_t const size_log2,
			                   Page_flags const &flags,
			                   void * const extra_space = 0) {
				return Arm::Section_table::
				insert_translation<Section_table>(vo, pa, size_log2, flags,
				                                  this, extra_space); }

			/**
			 * Insert translations for given area, do not permit displacement
			 *
			 * \param vo      virtual offset within this table
			 * \param s       area size
			 * \param io_mem  wether the area maps MMIO
			 */
			void map_core_area(addr_t vo, size_t s, bool const io_mem) {
				Arm::Section_table::map_core_area<Section_table>(vo, s, io_mem,
				                                                 this); }

			/***************
			 ** Accessors **
			 ***************/

			bool secure() { return _secure; }
	};
}


template <typename T>
static typename T::access_t
Arm::memory_region_attr(Page_flags const &flags)
{
	typedef typename T::Tex Tex;
	typedef typename T::C C;
	typedef typename T::B B;

	/*
	 * FIXME: upgrade to write-back & write-allocate when !d & c
	 */
	if(flags.device)
		return Tex::bits(2) | C::bits(0) | B::bits(0);

	if(flags.cacheable)
		return Tex::bits(5) | C::bits(0) | B::bits(1);

	return Tex::bits(6) | C::bits(1) | B::bits(0);
}

#endif /* _TLB__ARM_V7_H_ */


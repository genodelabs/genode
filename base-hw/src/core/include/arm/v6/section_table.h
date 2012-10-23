/*
 * \brief   Driver for ARMv6 section tables
 * \author  Martin Stein
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM_V6__SECTION_TABLE_H_
#define _INCLUDE__ARM_V6__SECTION_TABLE_H_

/* core includes */
#include <arm/section_table.h>

namespace Arm_v6
{
	using namespace Genode;

	/**
	 * First level translation table
	 */
	class Section_table : public Arm::Section_table
	{
		public:

			/**
			 * Link to second level translation-table
			 */
			struct Page_table_descriptor : Arm::Section_table::Page_table_descriptor
			{
				/**
				 * Compose descriptor value
				 */
				static access_t create(Arm::Page_table * const pt,
				                       Section_table *)
				{
					return Arm::Section_table::Page_table_descriptor::create(pt);
				}
			};

			/**
			 * Section translation descriptor
			 */
			struct Section : Arm::Section_table::Section
			{
				struct P : Bitfield<9, 1> { }; /* enable ECC */

				/**
				 * Compose descriptor value
				 */
				static access_t create(bool const w, bool const x,
				                       bool const k, bool const g,
				                       addr_t const pa,
				                       Section_table *)
				{
					return Arm::Section_table::Section::create(w, x, k, g, pa) |
					       P::bits(0);
				}
			};

			/**
			 * Insert one atomic translation into this table
			 *
			 * For details see 'Arm::Section_table::insert_translation'
			 */
			unsigned long insert_translation(addr_t const vo, addr_t const pa,
			                                 unsigned long const size_log2,
			                                 bool const w, bool const x,
			                                 bool const k, bool const g,
			                                 void * const extra_space = 0)
			{
				return Arm::Section_table::
				insert_translation<Section_table>(vo, pa, size_log2, w,
				                                  x, k, g, this, extra_space);
			}
	};
}

#endif /* _INCLUDE__ARM_V6__SECTION_TABLE_H_ */


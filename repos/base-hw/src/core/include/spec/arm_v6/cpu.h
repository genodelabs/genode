/*
 * \brief  CPU driver for core
 * \author Norman Feske
 * \author Martin stein
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__ARM_V6__CPU_H_
#define _CORE__INCLUDE__SPEC__ARM_V6__CPU_H_

/* core includes */
#include <spec/arm/cpu_support.h>
#include <assert.h>
#include <board.h>

namespace Genode { class Cpu; }


class Genode::Cpu : public Arm
{
	public:

		/**
		 * Cache type register
		 */
		struct Ctr : Arm::Ctr
		{
			struct P : Bitfield<23, 1> { }; /* page mapping restriction on */
		};

		/**
		 * System control register
		 */
		struct Sctlr : Arm::Sctlr
		{
			struct W  : Bitfield<3,1>  { }; /* enable write buffer */
			struct Dt : Bitfield<16,1> { }; /* global data TCM enable */
			struct It : Bitfield<18,1> { }; /* global instruction TCM enable */
			struct U  : Bitfield<22,1> { }; /* enable unaligned data access */
			struct Xp : Bitfield<23,1> { }; /* disable subpage AP bits */

			static void init()
			{
				access_t v = read();
				A::set(v, 0);
				V::set(v, 1);
				W::set(v, 1);
				Dt::set(v, 1);
				It::set(v, 1);
				U::set(v, 1);
				Xp::set(v, 1);
				write(v);
			}
		};

		/**
		 * If page descriptor bits [13:12] are restricted
		 */
		static bool restricted_page_mappings() {
			return Ctr::P::get(Ctr::read()); }

		/**
		 * Ensure that TLB insertions get applied
		 */
		void translation_table_insertions()
		{
			clean_invalidate_data_cache();
			invalidate_instr_cache();
			invalidate_tlb();
		}

		/**
		 * Post processing after a translation was added to a translation table
		 *
		 * \param addr  virtual address of the translation
		 * \param size  size of the translation
		 */
		static void translation_added(addr_t const addr, size_t const size);


		/*************
		 ** Dummies **
		 *************/

		static void wait_for_interrupt() { /* FIXME */ }
		static void data_synchronization_barrier() { /* FIXME */ }
		static void invalidate_control_flow_predictions() { /* FIXME */ }
};

#endif /* _CORE__INCLUDE__SPEC__ARM_V6__CPU_H_ */

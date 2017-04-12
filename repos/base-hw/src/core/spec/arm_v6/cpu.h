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

#ifndef _CORE__SPEC__ARM_V6__CPU_H_
#define _CORE__SPEC__ARM_V6__CPU_H_

/* core includes */
#include <spec/arm/cpu_support.h>

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
		 * If page descriptor bits [13:12] are restricted
		 */
		static bool restricted_page_mappings() {
			return Ctr::P::get(Ctr::read()); }


		/*************
		 ** Dummies **
		 *************/

		static void wait_for_interrupt() { /* FIXME */ }
};

#endif /* _CORE__SPEC__ARM_V6__CPU_H_ */

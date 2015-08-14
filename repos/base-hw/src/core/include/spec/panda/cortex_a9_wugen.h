/*
 * \brief   Cortex-A9 Wake-Up Generator
 * \author  Martin Stein
 * \date    2015-08-14
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef CORTEX_A9_WUGEN_H
#define CORTEX_A9_WUGEN_H

/* base includes */
#include <unmanaged_singleton.h>

/* Genode includes */
#include <drivers/board_base.h>

namespace Genode { class Cortex_a9_wugen; }

/**
 * Cortex_a9 Wake-Up Generator
 */
class Genode::Cortex_a9_wugen : Mmio
{
	friend Unmanaged_singleton_constructor;

	private:

		struct Aux_core_boot_0 : Register<0x800, 32> {
			struct Cpu1_status : Bitfield<2, 2> { }; };

		struct Aux_core_boot_1 : Register<0x804, 32> { };

		Cortex_a9_wugen() : Mmio(Board_base::CORTEX_A9_WUGEN_MMIO_BASE) { }

	public:

		static Cortex_a9_wugen * singleton() {
			return unmanaged_singleton<Cortex_a9_wugen>(); }

		/**
		 * Start CPU 1 with instruction pointer 'ip'
		 */
		void init_cpu_1(void * const ip)
		{
			write<Aux_core_boot_1>((addr_t)ip);
			write<Aux_core_boot_0::Cpu1_status>(1);
		}
};

#endif /* CORTEX_A9_WUGEN_H */

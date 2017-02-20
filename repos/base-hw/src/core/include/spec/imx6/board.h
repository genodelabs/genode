/*
 * \brief  Board driver
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-02-25
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__IMX6__BOARD_H_
#define _CORE__INCLUDE__SPEC__IMX6__BOARD_H_

/* core includes */
#include <spec/imx/board_support.h>
#include <spec/cortex_a9/board_support.h>

namespace Genode { class Board; }


class Genode::Board : public Imx::Board, public Cortex_a9::Board
{
	private:

		struct Src : Mmio
		{
			Src() : Mmio(SRC_MMIO_BASE) {}

			struct Scr  : Register<0x0,  32>
			{
				struct Core_1_reset  : Bitfield<14,1> {};
				struct Core_2_reset  : Bitfield<15,1> {};
				struct Core_3_reset  : Bitfield<16,1> {};
				struct Core_1_enable : Bitfield<22,1> {};
				struct Core_2_enable : Bitfield<23,1> {};
				struct Core_3_enable : Bitfield<24,1> {};
			};
			struct Gpr1 : Register<0x20, 32> {}; /* ep core 0 */
			struct Gpr3 : Register<0x28, 32> {}; /* ep core 1 */
			struct Gpr5 : Register<0x30, 32> {}; /* ep core 2 */
			struct Gpr7 : Register<0x38, 32> {}; /* ep core 3 */

			void entrypoint(void * entry)
			{
				write<Gpr3>((Gpr3::access_t)entry);
				write<Gpr5>((Gpr5::access_t)entry);
				write<Gpr7>((Gpr7::access_t)entry);
				Scr::access_t v = read<Scr>();
				Scr::Core_1_enable::set(v,1);
				Scr::Core_1_reset::set(v,1);
				Scr::Core_2_enable::set(v,1);
				Scr::Core_3_reset::set(v,1);
				Scr::Core_3_enable::set(v,1);
				Scr::Core_3_reset::set(v,1);
				write<Scr>(v);
			}
		} _src;

	public:

		void init()
		{
			Imx::Board::init();
			Cortex_a9::Board::init();
		}

		void wake_up_all_cpus(void *entry) { _src.entrypoint(entry); }
};

#endif /* _CORE__INCLUDE__SPEC__IMX6__BOARD_H_ */

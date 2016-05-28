/*
 * \brief   Snoop control unit
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2015-12-15
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/mmio.h>

/* local includes */
#include <board.h>

namespace Genode {
	class Scu;
}

class Genode::Scu : Genode::Mmio
{
	private:

		struct Cr : Register<0x0, 32>
		{
			struct Enable : Bitfield<0, 1> { };
		};

		struct Dcr : Register<0x30, 32>
		{
			struct Bit_0 : Bitfield<0, 1> { };
		};

		struct Iassr : Register<0xc, 32>
		{
			struct Cpu0_way : Bitfield<0, 4> { };
			struct Cpu1_way : Bitfield<4, 4> { };
			struct Cpu2_way : Bitfield<8, 4> { };
			struct Cpu3_way : Bitfield<12, 4> { };
		};

		Board &_board;

	public:

		Scu(Board & board) : Mmio(Board::SCU_MMIO_BASE), _board(board) { }

		void invalidate()
		{
			Iassr::access_t iassr = 0;
			for (Iassr::access_t way = 0; way <= Iassr::Cpu0_way::mask();
			     way++) {
				Iassr::Cpu0_way::set(iassr, way);
				Iassr::Cpu1_way::set(iassr, way);
				Iassr::Cpu2_way::set(iassr, way);
				Iassr::Cpu3_way::set(iassr, way);
				write<Iassr>(iassr);
			}
		}

		void enable()
		{
			if (_board.errata(Board::ARM_764369)) write<Dcr::Bit_0>(1);
			write<Cr::Enable>(1);
		}
};

/*
 * \brief  Driver for the SP810 system controller
 * \author Stefan Kalkowski
 * \date   2012-09-21
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__SRC__SERVER__VMM__810_H_
#define _BASE_HW__SRC__SERVER__VMM__810_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{

	class Sp810 : Mmio
	{
		private:

			struct Ctrl : public Register<0, 32>
			{
					struct Timer0_enable  : Bitfield<15,1> {};
					struct Timer1_enable  : Bitfield<17,1> {};
			};

		public:

			Sp810(addr_t const base) : Mmio(base) {}

			bool timer0() { return read<Ctrl::Timer0_enable>(); }
			bool timer1() { return read<Ctrl::Timer0_enable>(); }

			void enable_timer0() { write<Ctrl::Timer0_enable>(1); }
			void enable_timer1() { write<Ctrl::Timer0_enable>(1); }
	};
}

#endif /* _BASE_HW__SRC__SERVER__VMM__SP810_H_ */

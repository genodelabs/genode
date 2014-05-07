/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IMX53__PIC_H_
#define _IMX53__PIC_H_

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <pic_base.h>

namespace Imx53
{
	using namespace Genode;

	/**
	 * Programmable interrupt controller for core
	 */
	class Pic : public Pic_base
	{
		public:

			Pic()
			{
				for (unsigned i = 0; i <= MAX_INTERRUPT_ID; i++) {
					write<Intsec::Nonsecure>(0, i);
					write<Priority>(0, i);
				}

				write<Priomask::Mask>(0xff);
			}

			void unsecure(unsigned const i)
			{
				if (i <= MAX_INTERRUPT_ID) {
					write<Intsec::Nonsecure>(1, i);
					write<Priority>(0x80, i);
				}
			}

			void secure(unsigned const i)
			{
				if (i <= MAX_INTERRUPT_ID) {
					write<Intsec::Nonsecure>(0, i);
					write<Priority>(0, i);
				}
			}
	};
}

namespace Kernel { class Pic : public Imx53::Pic { }; }

#endif /* _IMX53__PIC_H_ */

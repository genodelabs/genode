/*
 * \brief  Pulse width modulation
 * \author Stefan Kalkowski
 * \date   2013-03-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__FRAMEBUFFER__SPEC__IMX53__PWM_H_
#define _DRIVERS__FRAMEBUFFER__SPEC__IMX53__PWM_H_

/* Genode includes */
#include <util/mmio.h>

struct Pwm : Genode::Mmio
{
	struct Control : Register<0x0, 32> {};
	struct Sample  : Register<0xc, 32> {};
	struct Period  : Register<0x10,32> {};

	Pwm(Genode::addr_t const mmio_base) : Genode::Mmio(mmio_base) { }

	void enable_display()
	{
		write<Period>(0x64);
		write<Sample>(0x64);
		write<Control>(0x3c20001);
	}
};

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__IMX53__PWM_H_ */

/*
 * \brief  Driver for the platform specific functionality for bcm2837
 * \author Tomasz Gajewski
 * \date   2019-12-28
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__PLATFORM__BCM2837_CONTROL_H_
#define _INCLUDE__DRIVERS__PLATFORM__BCM2837_CONTROL_H_

/* Genode includes */
#include <util/mmio.h>


namespace Genode { class Bcm2837_control; }


class Genode::Bcm2837_control : Mmio
{
	public:

		struct ControlRegister : public Register<0x0, 32>
		{
			struct CoreTimeClockSource : Bitfield<8,1> { };
			struct TimerIncrement     : Bitfield<9,1> { };
		};

		struct CoreTimerPrescaler : public Register<0x8, 32>
		{
		};

		inline Bcm2837_control(addr_t const base);

		inline void initialize_timer_frequency();
};


Genode::Bcm2837_control::Bcm2837_control(addr_t const base)
:
	Mmio(base)
{ }


void Genode::Bcm2837_control::initialize_timer_frequency()
{
	/*
	 * Set prescaler value to achieve divider value equal to 1.
	 * Value taken from chapter "3.1.1 Timer clock" from QA7_rev3.4.pdf
	 * document describing the BCM2836 chip.
	 */
	write<CoreTimerPrescaler>(0x80000000);
}


#endif /* _INCLUDE__DRIVERS__PLATFORM__BCM2837_CONTROL_H_ */

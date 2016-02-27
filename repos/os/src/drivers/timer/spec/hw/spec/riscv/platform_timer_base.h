/*
 * \brief  Dummy platform timer
 * \author Sebastian Sumpf
 * \date   2016-02-10
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _HW__RISCV__PLATFORM_TIMER_BASE_H_
#define _HW__RISCV__PLATFORM_TIMER_BASE_H_

class Platform_timer_base
{
	public:

	enum { IRQ };

	unsigned long tics_to_us(unsigned long const tics) const { return 0; }
	unsigned long us_to_tics(unsigned long const us)   const { return 0; }

	unsigned long max_value() const { return 0; }
	unsigned long value(bool & wrapped) const { return 0; }

	void run_and_wrap(unsigned long value) { }
};

#endif /* _HW__RISCV__PLATFORM_TIMER_BASE_H_ */

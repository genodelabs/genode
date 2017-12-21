/*
 * \brief  GPIO driver interface
 * \author Stefan Kalkowski
 * \date   2013-05-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPIO__DRIVER_H_
#define _INCLUDE__GPIO__DRIVER_H_

/* Genode includes */
#include <base/signal.h>

namespace Gpio { struct Driver; }


struct Gpio::Driver : Genode::Interface
{
	/**
	 * Set direction of GPIO pin, whether it's an input or output one
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void direction(unsigned gpio, bool input) = 0;

	/**
	 * Set output level (high or low)
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void write(unsigned gpio, bool enable) = 0;

	/**
	 * Read input level (high or low)
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual bool read(unsigned gpio) = 0;

	/**
	 * Enable/disable debounce logic for the GPIO pin
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void debounce_enable(unsigned gpio, bool enable) = 0;

	/**
	 * Set delay for debounce logic for the GPIO pin
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void debounce_time(unsigned gpio, unsigned long us) = 0;

	/**
	 * Set IRQ trigger state to falling edge-triggered
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void falling_detect(unsigned gpio) = 0;

	/**
	 * Set IRQ trigger state to rising edge-triggered
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void rising_detect(unsigned gpio) = 0;

	/**
	 * Set IRQ trigger state to high level-triggered
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void high_detect(unsigned gpio) = 0;

	/**
	 * Set IRQ trigger state to low level-triggered
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void low_detect(unsigned gpio) = 0;

	/**
	 * Enable/disable IRQ for specified GPIO pin
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void irq_enable(unsigned gpio, bool enable) = 0;

	/**
	 * Acknowledge IRQ for specified GPIO pin
	 *
	 * \param gpio  corresponding gpio pin number
	 */
	virtual void ack_irq(unsigned gpio) = 0;

	/**
	 * Register signal handler for interrupts
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void register_signal(unsigned gpio,
	                             Genode::Signal_context_capability cap) = 0;

	/**
	 * Unregister signal handler
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual void unregister_signal(unsigned gpio) = 0;

	/**
	 * Check whether GPIO number is valid
	 *
	 *\param gpio  corresponding gpio pin number
	 */
	virtual bool gpio_valid(unsigned gpio) = 0;
};

#endif /* _INCLUDE__GPIO__DRIVER_H_ */

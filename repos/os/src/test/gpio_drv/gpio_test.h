/*
 * \brief  Test of GPIO driver
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GPIO_TEST_H_
#define _GPIO_TEST_H_

#include <gpio_session/connection.h>
#include <base/signal.h>
#include <util/mmio.h>
#include <base/printf.h>

using namespace Genode;

class Gpio_test
{
	public:

		enum {
			LED1_GPIO   = 7,
			LED2_GPIO   = 8,
			BUTTON_GPIO = 121,
			GPIO4_IRQ   = 32 + 32,
		};

	private:

		Gpio::Connection _gpio_led1;
		Gpio::Connection _gpio_led2;
		Gpio::Connection _gpio_button;
		Gpio::Connection _gpio_irq4;

		Signal_receiver sig_rec;
		Signal_context  sig_ctx;

	public:

		Gpio_test();

		void wait_for_signal() {
			sig_rec.wait_for_signal(); }

		bool polling_test();
		bool irq_test();
};


Gpio_test::Gpio_test()
: _gpio_led1(LED1_GPIO),
  _gpio_led2(LED2_GPIO),
  _gpio_button(BUTTON_GPIO),
  _gpio_irq4(GPIO4_IRQ)
{
	/* initialize GPIO_121 */
	_gpio_button.debouncing(31*100);
	_gpio_button.irq_sigh(sig_rec.manage(&sig_ctx));
}


bool Gpio_test::polling_test()
{
	printf("---------- Polling test ----------\n");

	printf("\nPush and hold button...\n");

	_gpio_led1.write(true);
	_gpio_led2.write(false);

	volatile bool gpio_state;

	do {
		gpio_state = _gpio_button.read();
	} while (gpio_state);

	printf("OK\n");

	_gpio_led1.write(false);
	_gpio_led2.write(true);

	printf("\nRelease button...\n");

	do {
		gpio_state = _gpio_button.read();
	} while (!gpio_state);

	printf("OK\n");

	return true;
}

bool Gpio_test::irq_test()
{
	printf("---------- IRQ test ----------\n");

	_gpio_button.irq_type(Gpio::Session::FALLING_EDGE);
	_gpio_button.irq_enable(true);

	_gpio_led1.write(true);
	_gpio_led2.write(false);

	printf("\nPush and hold button...\n");

	wait_for_signal();

	_gpio_button.irq_enable(false);

	printf("OK\n");

	_gpio_button.irq_type(Gpio::Session::RISING_EDGE);
	_gpio_button.irq_enable(true);

	_gpio_led1.write(false);
	_gpio_led2.write(true);

	printf("\nRelease button...\n");

	wait_for_signal();

	_gpio_button.irq_enable(false);

	printf("OK\n");

	_gpio_button.irq_type(Gpio::Session::HIGH_LEVEL);
	return true;
}

#endif /* _GPIO_TEST_H_ */

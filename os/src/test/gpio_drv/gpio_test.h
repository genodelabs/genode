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
			LED1_GPIO        = 7,
			LED2_GPIO        = 8,
			BUTTON_GPIO     = 121,
			GPIO4_IRQ       = 32 + 32,
		};

	private:

		Gpio::Connection _gpio;

		Signal_receiver sig_rec;
		Signal_context  sig_ctx;

	public:
		Gpio_test();
		~Gpio_test();

		void wait_for_signal()
		{
			sig_rec.wait_for_signal();
		}

		bool polling_test();
		bool irq_test();
};


Gpio_test::Gpio_test()
{
	/* initialize GPIO_121 */
	_gpio.debouncing_time(BUTTON_GPIO, 31*100);
	_gpio.debounce_enable(BUTTON_GPIO, 1);

	_gpio.irq_sigh(sig_rec.manage(&sig_ctx), BUTTON_GPIO);
}


Gpio_test::~Gpio_test()
{
}


bool Gpio_test::polling_test()
{
	printf("---------- Polling test ----------\n");

	printf("\nPush and hold button...\n");
	_gpio.dataout(LED1_GPIO, true);
	_gpio.dataout(LED2_GPIO, false);

	volatile int gpio_state;

	do {
		gpio_state = _gpio.datain(BUTTON_GPIO);
	} while (gpio_state);

	printf("OK\n");

	_gpio.dataout(LED1_GPIO, false);
	_gpio.dataout(LED2_GPIO, true);

	printf("\nRelease button...\n");
	do {
		gpio_state = _gpio.datain(BUTTON_GPIO);
	} while (!gpio_state);

	printf("OK\n");

	return true;
}

bool Gpio_test::irq_test()
{
	printf("---------- IRQ test ----------\n");

	_gpio.falling_detect(BUTTON_GPIO, 1);
	_gpio.irq_enable(BUTTON_GPIO, 1);

	_gpio.dataout(LED1_GPIO, true);
	_gpio.dataout(LED2_GPIO, false);

	printf("\nPush and hold button...\n");

	wait_for_signal();

	_gpio.irq_enable(BUTTON_GPIO, 0);
	printf("OK\n");

	_gpio.falling_detect(BUTTON_GPIO, 0);
	_gpio.rising_detect(BUTTON_GPIO, 1);
	_gpio.irq_enable(BUTTON_GPIO, 1);

	_gpio.dataout(LED1_GPIO, false);
	_gpio.dataout(LED2_GPIO, true);

	printf("\nRelease button...\n");

	wait_for_signal();

	_gpio.irq_enable(BUTTON_GPIO, 0);
	printf("OK\n");

	_gpio.falling_detect(BUTTON_GPIO, 0);
	_gpio.rising_detect(BUTTON_GPIO, 0);
	return true;
}

#endif /* _GPIO_TEST_H_ */

/*
 * \brief  Virtual machine implementation
 * \author Martin Stein
 * \date   2015-06-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <vm.h>
#include <gpio_session/connection.h>

Gpio::Connection * led()
{
	static Gpio::Connection led(123);
	return &led;
}

void on_vmm_entry()
{
	led()->direction(Gpio::Session::OUT);
	led()->write(false);
}


void on_vmm_exit()
{
	led()->write(true);
}

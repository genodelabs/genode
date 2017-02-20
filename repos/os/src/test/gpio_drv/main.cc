/*
 * \brief  Test of GPIO driver
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/printf.h>

#include "gpio_test.h"

int main(int, char **)
{
	printf("--- Pandaboard button (GPIO_121) test ---\n");

	Gpio_test gpio_test;

	while(1)
	{
		gpio_test.polling_test();
		gpio_test.irq_test();
	}

	return 0;
}

/*
 * \brief  Provide detailed hardware Information
 * \author Martin Stein
 * \date   2014-10-20
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base includes */
#include <base/printf.h>

/* core includes */
#include <kernel/test.h>

using namespace Genode;

void info();

/**
 * Main routine
 */
void Kernel::test()
{
	printf("\n");
	info();
	printf("------ End ------\n");
	while (1) ;
}

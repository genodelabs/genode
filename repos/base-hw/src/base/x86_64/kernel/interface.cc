/*
 * \brief  Interface between kernel and userland
 * \author Martin Stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <kernel/interface.h>

using namespace Kernel;


/******************
 ** Kernel calls **
 ******************/

Call_ret Kernel::call(Call_arg arg_0)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}


Call_ret Kernel::call(Call_arg arg_0,
                      Call_arg arg_1,
                      Call_arg arg_2,
                      Call_arg arg_3,
                      Call_arg arg_4,
                      Call_arg arg_5)
{
	PDBG("syscall binding not implemented");
	for (;;);
	return 0;
}

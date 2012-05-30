/*
 * \brief  Syscall declarations specific for ARM V7A systems
 * \author Martin Stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM_V7A__BASE__SYSCALL_H_
#define _INCLUDE__ARM_V7A__BASE__SYSCALL_H_

/* Genode includes */
#include <base/stdint.h>

namespace Kernel
{
	typedef Genode::uint32_t Syscall_arg;
	typedef Genode::uint32_t Syscall_ret;

	/*****************************************************************
	 ** Syscall with 1 to 6 arguments                               **
	 **                                                             **
	 ** These functions must not be inline to ensure that objects,  **
	 ** wich are referenced by arguments, are tagged as "used" even **
	 ** though only the pointer gets handled in here.               **
	 *****************************************************************/

	Syscall_ret syscall(Syscall_arg arg_0);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1,
	                    Syscall_arg arg_2);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1,
	                    Syscall_arg arg_2,
	                    Syscall_arg arg_3);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1,
	                    Syscall_arg arg_2,
	                    Syscall_arg arg_3,
	                    Syscall_arg arg_4);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1,
	                    Syscall_arg arg_2,
	                    Syscall_arg arg_3,
	                    Syscall_arg arg_4,
	                    Syscall_arg arg_5);
}

#endif /* _INCLUDE__ARM_V7A__BASE__SYSCALL_H_ */


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

#ifndef _INCLUDE__ARM__BASE__SYSCALL_H_
#define _INCLUDE__ARM__BASE__SYSCALL_H_

/* Genode includes */
#include <base/stdint.h>

namespace Kernel
{
	typedef Genode::uint32_t Syscall_arg;
	typedef Genode::uint32_t Syscall_ret;
}

#endif /* _INCLUDE__ARM__BASE__SYSCALL_H_ */


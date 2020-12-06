/*
 * \brief  Collection of L4/Fiasco kernel bindings
 * \author Norman Feske
 * \date   2020-12-06
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FIASCO__SYSCALL_H_
#define _INCLUDE__FIASCO__SYSCALL_H_

namespace Fiasco {
#include <l4/sys/types.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kernel.h>
#include <l4/sys/kip.h>
#include <l4/sys/utcb.h>
#include <l4/sys/ktrace.h>
#include <l4/sys/syscalls.h>
#include <l4/sigma0/sigma0.h>
}

#endif /* _INCLUDE__FIASCO__SYSCALL_H_ */

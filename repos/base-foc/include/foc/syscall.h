/*
 * \brief  Collection of Fiasco.OC kernel bindings
 * \author Norman Feske
 * \date   2020-11-27
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC__SYSCALL_H_
#define _INCLUDE__FOC__SYSCALL_H_

namespace Foc {
#include <l4/sys/types.h>
#include <l4/sys/kip>
#include <l4/sys/kdebug.h>
#include <l4/sys/cache.h>
#include <l4/sys/consts.h>
#include <l4/sys/utcb.h>
#include <l4/sys/task.h>
#include <l4/sys/ipc.h>
#include <l4/sys/thread.h>
#include <l4/sys/factory.h>
#include <l4/sys/irq.h>
#include <l4/sys/debugger.h>
#include <l4/sys/icu.h>
#include <l4/sys/ktrace.h>
#include <l4/sys/scheduler.h>
#include <l4/sigma0/sigma0.h>
}

#endif /* _INCLUDE__FOC__SYSCALL_H_ */

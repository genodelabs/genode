/*
 * \brief   Kernel lock
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__SMP__KERNEL__LOCK_H_
#define _CORE__SPEC__SMP__KERNEL__LOCK_H_

#include <hw/spin_lock.h>

namespace Kernel
{
	using Lock = Hw::Spin_lock;

	Lock & data_lock();
}

#endif /* _CORE__SPEC__SMP__KERNEL__LOCK_H_ */

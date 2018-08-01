/*
 * \brief   Cpu class implementation specific to SMP
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/lock.h>
#include <kernel/kernel.h>
#include <kernel/cpu.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>


/* spin-lock used to synchronize kernel access of different cpus */
Kernel::Lock & Kernel::data_lock() {
	return *unmanaged_singleton<Kernel::Lock>(); }

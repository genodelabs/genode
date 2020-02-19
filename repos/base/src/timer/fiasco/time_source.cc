/*
 * \brief  Time source that uses sleeping by the means of the kernel
 * \author Christian Helmuth
 * \author Norman Feske
 * \author Martin Stein
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/misc_math.h>
#include <base/attached_rom_dataspace.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/kip.h>
}

/*
 * On L4/Fiasco, the KIP layout is defined in 'kernel.h', which does not exist
 * on Fiasco.OC. We test for 'L4_SYS_KIP_H__' to check for the L4/Fiasco case
 * and include 'kernel.h'. This works because the Fiasco.OC headers do not use
 * include guards ('L4_SYS_KIP_H__' is undefined on Fiasco.OC).
 */
#ifdef L4_SYS_KIP_H__
namespace Fiasco {
#include <l4/sys/kernel.h>
}
#endif /* L4_SYS_KIP_H__ */

/* local includes */
#include <time_source.h>

using namespace Fiasco;
using Microseconds = Genode::Microseconds;
using Duration     = Genode::Duration;
using Genode::uint64_t;


static l4_timeout_s mus_to_timeout(uint64_t mus)
{
	if (mus == 0)
		return L4_IPC_TIMEOUT_0;
	else if (mus == ~(uint64_t)0)
		return L4_IPC_TIMEOUT_NEVER;

	long e = Genode::log2((unsigned long)mus) - 7;
	unsigned long m;
	if (e < 0) e = 0;
	m = mus / (1UL << e);

	/* check corner case */
	if ((e > 31 ) || (m > 1023)) {
		Genode::warning("invalid timeout ", mus, ", using max. values");
		e = 0;
		m = 1023;
	}
	return l4_timeout_rel(m, e);
}


Microseconds Timer::Time_source::max_timeout() const
{
	Genode::Mutex::Guard lock_guard(_mutex);
	return Microseconds(1000 * 1000 * 100);
}


Duration Timer::Time_source::curr_time()
{
	Genode::Mutex::Guard mutex_guard(_mutex);
	static Genode::Attached_rom_dataspace kip_ds(_env, "l4v2_kip");
	static Fiasco::l4_kernel_info_t * const kip =
		kip_ds.local_addr<Fiasco::l4_kernel_info_t>();

#ifdef L4_SYS_KIP_H__
	Fiasco::l4_cpu_time_t const clock = kip->clock;
#else
	Fiasco::l4_cpu_time_t const clock = Fiasco::l4_kip_clock(kip);
#endif

	return Duration(Microseconds(clock));
}


void Timer::Time_source::_usleep(uint64_t usecs) {
	l4_ipc_sleep(l4_timeout(L4_IPC_TIMEOUT_NEVER, mus_to_timeout(usecs))); }

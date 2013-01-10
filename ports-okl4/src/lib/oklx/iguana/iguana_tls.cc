/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-07
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <util/string.h>

enum {TLS_ERRNO_KEY, TLS_TIMER_KEY, TLS_NAMING_KEY,
      TLS_SYNCH_BITS_KEY, TLS_UNUSED1, TLS_UNUSED2, TLS_THREAD_ID};

extern "C" {
#include <iguana/tls.h>
#include <iguana/thread.h>
#include <l4/utcb.h>


	void __tls_init(void *tls_buffer)
	{
		Genode::memset(tls_buffer, 0, 32 * sizeof(Genode::addr_t));
		__L4_TCR_Set_ThreadLocalStorage((L4_Word_t)tls_buffer);
		((L4_Word_t *)tls_buffer)[TLS_THREAD_ID] =
			thread_l4tid(thread_myself()).raw;
	}

} // extern "C"

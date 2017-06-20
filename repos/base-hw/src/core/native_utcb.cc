/*
 * \brief  Core-local main thread's UTCB address
 * \author Stefan Kalkowski
 * \date   2017-06-21
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/native_utcb.h>

#include <hw/memory_map.h>

Genode::Native_utcb * Genode::utcb_main_thread() {
	return (Genode::Native_utcb*) Hw::Mm::core_utcb_main_thread().base; }


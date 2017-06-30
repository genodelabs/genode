/*
 * \brief   Initialization code for bootstrap
 * \author  Stefan Kalkowski
 * \date    2016-09-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local */
#include <platform.h>

/* base includes */
#include <base/internal/globals.h>
#include <base/internal/unmanaged_singleton.h>

Bootstrap::Platform & Bootstrap::platform() {
	return *unmanaged_singleton<Bootstrap::Platform>(); }

extern "C" void init() __attribute__ ((noreturn));

extern "C" void init()
{
	Bootstrap::Platform & p = Bootstrap::platform();
	p.start_core(p.enable_mmu());
}

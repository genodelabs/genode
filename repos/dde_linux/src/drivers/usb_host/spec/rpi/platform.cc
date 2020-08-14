/*
 * \brief  USB initialization for Raspberry Pi
 * \author Norman Feske
 * \date   2013-09-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* emulation */
#include <platform.h>
#include <lx_emul.h>

/* dwc-otg */
#define new new_
#include <dwc_otg_dbg.h>
#undef new

using namespace Genode;


/*******************
 ** Init function **
 *******************/

extern "C" void module_dwc_otg_driver_init();
extern bool fiq_enable, fiq_fsm_enable;

void platform_hcd_init(Genode::Env &env, Services *services)
{
	/* disable fiq optimization */
	fiq_enable = false;
	fiq_fsm_enable = false;

	module_dwc_otg_driver_init();
	lx_platform_device_init();
}

/*
 * \brief  Wireless network driver Linux port
 * \author Josef Soentgen
 * \date   2022-02-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <genode_c_api/uplink.h>

/* DDE Linux includes */
#include <lx_emul/init.h>
#include <lx_emul/page_virt.h>
#include <lx_emul/task.h>
#include <lx_kit/env.h>
#include <lx_kit/init.h>
#include <lx_user/io.h>

/* local includes */
#include "lx_user.h"

using namespace Genode;


extern "C" int  lx_emul_rfkill_get_any(void);
extern "C" void lx_emul_rfkill_switch_all(int blocked);

static Genode::Signal_context_capability _rfkill_sigh_cap;


bool _wifi_get_rfkill(void)
{
	/*
	 * It is safe to call this from non EP threads as we
	 * only query a variable.
	 */
	return lx_emul_rfkill_get_any();
}


void _wifi_set_rfkill(bool blocked)
{
	if (!rfkill_task_struct_ptr)
		return;

	lx_emul_rfkill_switch_all(blocked);

	lx_emul_task_unblock(rfkill_task_struct_ptr);
	Lx_kit::env().scheduler.schedule();

	/*
	 * We have to open the device again after unblocking
	 * as otherwise we will get ENETDOWN. So unblock the uplink
	 * task _afterwards_ because there we call * 'dev_open()'
	 * unconditionally and that will bring the netdevice UP again.
	 */
	lx_emul_task_unblock(uplink_task_struct_ptr);
	Lx_kit::env().scheduler.schedule();

	Genode::Signal_transmitter(_rfkill_sigh_cap).submit();
}


bool wifi_get_rfkill(void)
{
	return _wifi_get_rfkill();
}


extern "C" unsigned int wifi_ifindex(void)
{
	/* TODO replace with actual qyery */
	return 2;
}


extern "C" char const *wifi_ifname(void)
{
	/* TODO replace with actual qyery */
	return "wlan0";
}

struct Wlan
{
	Env                    &_env;
	Io_signal_handler<Wlan> _signal_handler { _env.ep(), *this,
	                                          &Wlan::_handle_signal };

	void _handle_signal()
	{
		if (uplink_task_struct_ptr) {
			lx_emul_task_unblock(uplink_task_struct_ptr);
			Lx_kit::env().scheduler.schedule();
		}

		genode_uplink_notify_peers();
	}

	Wlan(Env &env) : _env { env }
	{
		genode_uplink_init(genode_env_ptr(_env),
		                   genode_allocator_ptr(Lx_kit::env().heap),
		                   genode_signal_handler_ptr(_signal_handler));

		lx_emul_start_kernel(nullptr);
	}
};


Genode::Blockade *wpa_blockade;


void wifi_init(Genode::Env      &env,
               Genode::Blockade &blockade)
{
	wpa_blockade = &blockade;

	static Wlan wlan(env);
}


void wifi_set_rfkill_sigh(Genode::Signal_context_capability cap)
{
	_rfkill_sigh_cap = cap;
}

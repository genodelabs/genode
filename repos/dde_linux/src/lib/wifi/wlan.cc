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
#include <net/mac_address.h>
#include <genode_c_api/uplink.h>
#include <genode_c_api/mac_address_reporter.h>


/* DDE Linux includes */
#include <lx_emul/init.h>
#include <lx_emul/page_virt.h>
#include <lx_emul/task.h>
#include <lx_kit/env.h>
#include <lx_kit/init.h>
#include <lx_user/io.h>

/* wifi includes */
#include <wifi/firmware.h>
#include <wifi/rfkill.h>

/* local includes */
#include "lx_user.h"
#include "dtb_helper.h"

using namespace Genode;

/* RFKILL handling */

extern "C" int  lx_emul_rfkill_get_any(void);
extern "C" void lx_emul_rfkill_switch_all(int blocked);


bool _wifi_get_rfkill(void)
{
	/*
	 * It is safe to call this from non EP threads as we
	 * only query a variable.
	 */
	return lx_emul_rfkill_get_any();
}


struct Rfkill_helper
{
	Wifi::Rfkill_notification_handler &_handler;

	Rfkill_helper(Wifi::Rfkill_notification_handler &handler)
	:
		_handler { handler }
	{ }

	void submit_notification()
	{
		_handler.rfkill_notify();
	}
};

Constructible<Rfkill_helper> rfkill_helper { };


void Wifi::set_rfkill(bool blocked)
{
	if (!rfkill_task_struct_ptr)
		return;

	lx_emul_rfkill_switch_all(blocked);

	lx_emul_task_unblock(rfkill_task_struct_ptr);
	Lx_kit::env().scheduler.execute();

	/*
	 * We have to open the device again after unblocking
	 * as otherwise we will get ENETDOWN. So unblock the uplink
	 * task _afterwards_ because there we call * 'dev_open()'
	 * unconditionally and that will bring the netdevice UP again.
	 */
	lx_emul_task_unblock(uplink_task_struct_ptr);
	Lx_kit::env().scheduler.execute();

	if (rfkill_helper.constructed())
		rfkill_helper->submit_notification();
}


bool Wifi::rfkill_blocked(void)
{
	return _wifi_get_rfkill();
}


/* Firmware access, move to object later */

struct task_struct;

struct Firmware_helper
{
	Firmware_helper(Firmware_helper const&) = delete;
	Firmware_helper & operator = (Firmware_helper const&) = delete;

	void *_waiting_task { nullptr };
	void *_calling_task { nullptr };

	Genode::Signal_handler<Firmware_helper> _response_handler;

	void _handle_response()
	{
		if (_calling_task)
			lx_emul_task_unblock((struct task_struct*)_calling_task);

		Lx_kit::env().scheduler.execute();
	}

	Wifi::Firmware_request_handler &_request_handler;

	struct Request : Wifi::Firmware_request
	{
		Genode::Signal_context &_response_handler;

		Request(Genode::Signal_context &sig_ctx)
		:
			_response_handler { sig_ctx }
		{ }

		void submit_response() override
		{
			switch (state) {
			case Firmware_request::State::PROBING:
				state = Firmware_request::State::PROBING_COMPLETE;
				break;
			case Firmware_request::State::REQUESTING:
				state = Firmware_request::State::REQUESTING_COMPLETE;
				break;
			default:
				return;
			}
			_response_handler.local_submit();
		}
	};

	using S = Wifi::Firmware_request::State;

	Request _request { _response_handler };

	void _update_waiting_task()
	{
		if (_waiting_task)
			if (_waiting_task != lx_emul_task_get_current())
				warning("Firmware_request: already waiting task is not current task");

		_waiting_task = lx_emul_task_get_current();
	}

	void _submit_request_and_wait_for(Wifi::Firmware_request::State state)
	{
		_calling_task = lx_emul_task_get_current();
		_request_handler.submit_request();

		do {
			lx_emul_task_schedule(true);
		} while (_request.state != state);
	}

	void _wait_until_pending_request_finished()
	{
		while (_request.state != S::INVALID) {

			_update_waiting_task();
			lx_emul_task_schedule(true);
		}
	}

	void _wakeup_any_waiting_request()
	{
		_request.state = S::INVALID;
		if (_waiting_task) {
			lx_emul_task_unblock((struct task_struct*)_waiting_task);
			_waiting_task = nullptr;
		}
		_calling_task = nullptr;
	}

	Firmware_helper(Genode::Entrypoint &ep,
	                       Wifi::Firmware_request_handler &request_handler)
	:
		_response_handler { ep, *this, &Firmware_helper::_handle_response },
		_request_handler  { request_handler }
	{ }

	size_t perform_probing(char const *name)
	{
		_wait_until_pending_request_finished();

		_request.name    = name;
		_request.state   = S::PROBING;
		_request.dst     = nullptr;
		_request.dst_len = 0;

		_submit_request_and_wait_for(S::PROBING_COMPLETE);

		size_t const fw_length = _request.success ? _request.fw_len : 0;

		_wakeup_any_waiting_request();

		return fw_length;
	}

	int perform_requesting(char const *name, char *dst, size_t dst_len)
	{
		_wait_until_pending_request_finished();

		_request.name    = name;
		_request.state   = S::REQUESTING;
		_request.dst     = dst;
		_request.dst_len = dst_len;

		_submit_request_and_wait_for(S::REQUESTING_COMPLETE);

		bool const success = _request.success;

		_wakeup_any_waiting_request();

		return success ? 0 : -1;
	}

	Wifi::Firmware_request *request()
	{
		return &_request;
	}
};


Constructible<Firmware_helper> firmware_helper { };


size_t _wifi_probe_firmware(char const *name)
{
	if (firmware_helper.constructed())
		return firmware_helper->perform_probing(name);

	return 0;
}


int _wifi_request_firmware(char const *name, char *dst, size_t dst_len)
{
	if (firmware_helper.constructed())
		return firmware_helper->perform_requesting(name, dst, dst_len);

	return -1;
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


/* used from socket_call.cc */
void _wifi_report_mac_address(Net::Mac_address const &mac_address)
{
	struct genode_mac_address address;

	mac_address.copy(&address);
	genode_mac_address_register("wlan0", address);
}


struct Wlan
{
	Env                    &_env;
	Io_signal_handler<Wlan> _signal_handler { _env.ep(), *this,
	                                          &Wlan::_handle_signal };

	Dtb_helper _dtb_helper { _env };

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
		genode_mac_address_reporter_init(env, Lx_kit::env().heap);

		{
			/*
			 * Query the configuration once at start-up to enable
			 * the reporter. The actual reporting will be done
			 * once by 'genode_mac_address_register()'.
			 */
			Attached_rom_dataspace _config_rom { _env, "config" };
			genode_mac_address_reporter_config(_config_rom.xml());
		}

		genode_uplink_init(genode_env_ptr(_env),
		                   genode_allocator_ptr(Lx_kit::env().heap),
		                   genode_signal_handler_ptr(_signal_handler));

		lx_emul_start_kernel(_dtb_helper.dtb_ptr());
	}
};


static Blockade *wpa_blockade;


extern "C" void wakeup_wpa()
{
	static bool called_once = false;
	if (called_once)
		return;

	wpa_blockade->wakeup();
	called_once = true;
}


void wifi_init(Env &env, Blockade &blockade)
{
	wpa_blockade = &blockade;

	static Wlan wlan(env);
}


void Wifi::rfkill_establish_handler(Wifi::Rfkill_notification_handler &handler)
{
	rfkill_helper.construct(handler);
}


void Wifi::firmware_establish_handler(Wifi::Firmware_request_handler &request_handler)
{
	firmware_helper.construct(Lx_kit::env().env.ep(), request_handler);
}


Wifi::Firmware_request *Wifi::firmware_get_request()
{
	if (firmware_helper.constructed())
		return firmware_helper->request();

	return nullptr;
}

/*
 * \brief  Linux emulation environment firmware handling
 * \author Josef Soentgen
 * \date   2026-03-04
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

/* Genode includes */
#include <base/log.h>

/* lx emul/kit includes */
#include <lx_kit/env.h>
#include <lx_emul/task.h>

/* local includes */
#include "firmware.h"

using namespace Genode;


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

	Lx_kit::Firmware_request_handler &_request_handler;

	struct Request : Lx_kit::Firmware_request
	{
		Genode::Signal_context &_response_handler;

		Request(Genode::Signal_context &sig_ctx)
		: _response_handler { sig_ctx } { }

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

	using S = Lx_kit::Firmware_request::State;

	Request _request { _response_handler };

	void _update_waiting_task()
	{
		if (_waiting_task)
			if (_waiting_task != lx_emul_task_get_current())
				warning("Firmware_request: already waiting task is not current task");

		_waiting_task = lx_emul_task_get_current();
	}

	void _submit_request_and_wait_for(Lx_kit::Firmware_request::State state)
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
	                Lx_kit::Firmware_request_handler &request_handler)
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

	Lx_kit::Firmware_request *request() {
		return &_request; }
};


static Firmware_helper *_firmware_helper_ptr;


/*
 * Firmware handling
 */

void Lx_kit::firmware_establish_handler(Lx_kit::Firmware_request_handler &request_handler)
{
	static Firmware_helper inst(Lx_kit::env().env.ep(), request_handler);
	_firmware_helper_ptr = &inst;
}


Lx_kit::Firmware_request *Lx_kit::firmware_get_request()
{
	return _firmware_helper_ptr ? _firmware_helper_ptr->request()
	                            : nullptr;
}


static size_t probe_firmware(char const *name)
{
	return _firmware_helper_ptr ? _firmware_helper_ptr->perform_probing(name)
	                            : 0;
}


static int request_firmware(char const *name, char *dst, size_t dst_len)
{
	return _firmware_helper_ptr ? _firmware_helper_ptr->perform_requesting(name, dst, dst_len)
	                            : -1;
}


/*
 * lx_emul firmware functions
 */

extern "C" int lx_emul_request_firmware_nowait(const char *name, void **dest,
                                               size_t *result, bool /* warn */)
{
	if (!dest || !result)
		return -1;

	size_t const fw_size = probe_firmware(name);

	if (!fw_size)
		return -1;

	/* use allocator because fw is too big for slab */
	char *data = (char*)Lx_kit::env().heap.alloc(fw_size);
	if (!data)
		return -1;

	if (request_firmware(name, data, fw_size)) {
		error("could not request firmware ", name);
		Lx_kit::env().heap.free(data, fw_size);
		return -1;
	}

	*dest   = data;
	*result = fw_size;

	return 0;
}


extern "C" void lx_emul_release_firmware(void const *data, size_t size)
{
	Lx_kit::env().heap.free(const_cast<void *>(data), size);
}

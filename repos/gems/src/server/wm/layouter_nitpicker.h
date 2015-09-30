/*
 * \brief  Nitpicker service provided to layouter
 * \author Norman Feske
 * \date   2015-06-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LAYOUTER_NITPICKER_H_
#define _LAYOUTER_NITPICKER_H_

/* Genode includes */
#include <os/server.h>
#include <input/component.h>

namespace Wm { using Server::Entrypoint; }


namespace Wm {
	struct Layouter_nitpicker_session;
	struct Layouter_nitpicker_service;
}


struct Wm::Layouter_nitpicker_session : Genode::Rpc_object<Nitpicker::Session>
{
	typedef Nitpicker::View_capability      View_capability;
	typedef Nitpicker::Session::View_handle View_handle;

	Input::Session_capability _input_session_cap;

	Attached_ram_dataspace _command_ds;

	Framebuffer::Mode const _mode;

	Layouter_nitpicker_session(Genode::Ram_session &ram,
	                           Input::Session_capability input_session_cap,
	                           Framebuffer::Mode mode)
	:
		_input_session_cap(input_session_cap),
		_command_ds(&ram, 4096),
		_mode(mode)
	{ }


	/*********************************
	 ** Nitpicker session interface **
	 *********************************/
	
	Framebuffer::Session_capability framebuffer_session() override
	{
		return Framebuffer::Session_capability();
	}

	Input::Session_capability input_session() override
	{
		return _input_session_cap;
	}

	View_handle create_view(View_handle) override { return View_handle(); }

	void destroy_view(View_handle) override { }

	View_handle view_handle(View_capability, View_handle) override
	{
		return View_handle();
	}

	View_capability view_capability(View_handle) override
	{
		return View_capability();
	}

	void release_view_handle(View_handle) override { }

	Genode::Dataspace_capability command_dataspace() override
	{
		return _command_ds.cap();
	}

	void execute() override { }

	Framebuffer::Mode mode() override { return _mode; }

	void mode_sigh(Genode::Signal_context_capability) override { }

	void buffer(Framebuffer::Mode, bool) override { }

	void focus(Genode::Capability<Nitpicker::Session>) { }
};

#endif /* _LAYOUTER_NITPICKER_H_ */

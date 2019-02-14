/*
 * \brief  Nitpicker service provided to layouter
 * \author Norman Feske
 * \date   2015-06-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LAYOUTER_NITPICKER_H_
#define _LAYOUTER_NITPICKER_H_

/* Genode includes */
#include <input/component.h>
#include <nitpicker_session/connection.h>

namespace Wm {
	struct Layouter_nitpicker_session;
	struct Layouter_nitpicker_service;
}


struct Wm::Layouter_nitpicker_session : Genode::Rpc_object<Nitpicker::Session>
{
	typedef Nitpicker::View_capability      View_capability;
	typedef Nitpicker::Session::View_handle View_handle;

	Input::Session_capability _input_session_cap;

	/*
	 * Nitpicker session solely used to supply the nitpicker mode to the
	 * layouter
	 */
	Nitpicker::Connection _mode_sigh_nitpicker;

	Genode::Signal_context_capability _mode_sigh { };

	Attached_ram_dataspace _command_ds;

	Layouter_nitpicker_session(Genode::Env &env,
	                           Input::Session_capability input_session_cap)
	:
		_input_session_cap(input_session_cap),
		_mode_sigh_nitpicker(env), _command_ds(env.ram(), env.rm(), 4096)
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

	Framebuffer::Mode mode() override { return _mode_sigh_nitpicker.mode(); }

	void mode_sigh(Genode::Signal_context_capability sigh) override
	{
		/*
		 * Remember signal-context capability to keep NOVA from revoking
		 * transitive delegations of the capability.
		 */
		_mode_sigh = sigh;

		_mode_sigh_nitpicker.mode_sigh(sigh);
	}

	void buffer(Framebuffer::Mode, bool) override { }

	void focus(Genode::Capability<Nitpicker::Session>) override { }
};

#endif /* _LAYOUTER_NITPICKER_H_ */

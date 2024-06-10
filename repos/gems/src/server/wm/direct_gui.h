/*
 * \brief  Pass-through GUI service announced to the outside world
 * \author Norman Feske
 * \date   2015-09-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DIRECT_GUI_H_
#define _DIRECT_GUI_H_

/* Genode includes */
#include <os/session_policy.h>
#include <gui_session/connection.h>

namespace Wm { class Direct_gui_session; }


class Wm::Direct_gui_session : public Genode::Rpc_object<Gui::Session>
{
	private:

		Genode::Session_label _session_label;
		Gui::Connection       _session;

	public:

		/**
		 * Constructor
		 */
		Direct_gui_session(Genode::Env &env, Genode::Session_label const &session_label)
		:
			_session_label(session_label),
			_session(env, _session_label.string())
		{ }

		void upgrade(char const *args)
		{
			size_t const ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			_session.upgrade_ram(ram_quota);
		}


		/***************************
		 ** GUI session interface **
		 ***************************/
		
		Framebuffer::Session_capability framebuffer_session() override
		{
			return _session.framebuffer_session();
		}

		Input::Session_capability input_session() override
		{
			return _session.input_session();
		}

		View_handle create_view() override
		{
			return _session.create_view();
		}

		View_handle create_child_view(View_handle parent) override
		{
			return _session.create_child_view(parent);
		}

		void destroy_view(View_handle view) override
		{
			_session.destroy_view(view);
		}

		View_handle view_handle(Gui::View_capability view_cap, View_handle handle) override
		{
			return _session.view_handle(view_cap, handle);
		}

		Gui::View_capability view_capability(View_handle view) override
		{
			return _session.view_capability(view);
		}

		void release_view_handle(View_handle view) override
		{
			_session.release_view_handle(view);
		}

		Genode::Dataspace_capability command_dataspace() override
		{
			return _session.command_dataspace();
		}

		void execute() override
		{
			_session.execute();
		}

		Framebuffer::Mode mode() override
		{
			return _session.mode();
		}

		void mode_sigh(Genode::Signal_context_capability sigh) override
		{
			_session.mode_sigh(sigh);
		}

		void buffer(Framebuffer::Mode mode, bool use_alpha) override
		{
			_session.buffer(mode, use_alpha);
		}

		void focus(Genode::Capability<Gui::Session> session) override
		{
			_session.focus(session);
		}
};

#endif /* _DIRECT_GUI_H_ */

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

		Genode::Env &_env;

		Genode::Session_label _label;

		Genode::Connection<Gui::Session> _connection {
			_env, _label, Genode::Ram_quota { 36*1024 }, /* Args */ { } };

		Gui::Session_client _session { _connection.cap() };

		using View_capability = Gui::View_capability;
		using View_id         = Gui::View_id;

	public:

		/**
		 * Constructor
		 */
		Direct_gui_session(Genode::Env &env, Genode::Session_label const &label)
		:
			_env(env), _label(label)
		{ }

		void upgrade(char const *args)
		{
			size_t const ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			_connection.upgrade_ram(ram_quota);
		}


		/***************************
		 ** GUI session interface **
		 ***************************/
		
		Framebuffer::Session_capability framebuffer() override
		{
			return _session.framebuffer();
		}

		Input::Session_capability input() override
		{
			return _session.input();
		}

		View_result view(View_id id, View_attr const &attr) override
		{
			return _session.view(id, attr);
		}

		Child_view_result child_view(View_id id, View_id parent, View_attr const &attr) override
		{
			return _session.child_view(id, parent, attr);
		}

		void destroy_view(View_id view) override
		{
			_session.destroy_view(view);
		}

		View_id_result view_id(View_capability view_cap, View_id id) override
		{
			return _session.view_id(view_cap, id);
		}

		View_capability view_capability(View_id view) override
		{
			return _session.view_capability(view);
		}

		void release_view_id(View_id view) override
		{
			_session.release_view_id(view);
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

		Buffer_result buffer(Framebuffer::Mode mode, bool use_alpha) override
		{
			return _session.buffer(mode, use_alpha);
		}

		void focus(Genode::Capability<Gui::Session> session) override
		{
			_session.focus(session);
		}
};

#endif /* _DIRECT_GUI_H_ */

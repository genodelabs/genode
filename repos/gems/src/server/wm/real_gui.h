/*
 * \brief  Wrapped GUI session
 * \author Norman Feske
 * \date   2024-08-05
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REAL_GUI_H_
#define _REAL_GUI_H_

/* Genode includes */
#include <base/connection.h>
#include <base/attached_dataspace.h>
#include <gui_session/client.h>

namespace Wm { struct Real_gui; }


struct Wm::Real_gui
{
	private:

		Genode::Env &_env;

	public:

		Genode::Session_label const &label;

		using Command_buffer = Gui::Session::Command_buffer;

	public:

		Real_gui(Genode::Env &env, Genode::Session_label const &label)
		:
			_env(env), label(label)
		{ }

		Genode::Connection<Gui::Session> connection {
			_env, label, Genode::Ram_quota { 36*1024 }, /* Args */ { } };

		Gui::Session_client session { connection.cap() };

	private:

		Genode::Attached_dataspace _command_ds { _env.rm(), session.command_dataspace() };

		Command_buffer &_command_buffer { *_command_ds.local_addr<Command_buffer>() };

	public:

		template <typename CMD>
		void enqueue(auto &&... args) { enqueue(Gui::Session::Command( CMD { args... } )); }

		void enqueue(Gui::Session::Command const &command)
		{
			if (_command_buffer.full())
				execute();

			_command_buffer.enqueue(command);
		}

		void execute()
		{
			session.execute();
			_command_buffer.reset();
		}
};

#endif /* _REAL_GUI_H_ */

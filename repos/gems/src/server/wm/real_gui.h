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

namespace Wm { struct Real_gui; }


struct Wm::Real_gui
{
	private:

		Env &_env;

	public:

		Session_label const &label;

		using Command_buffer = Gui::Session::Command_buffer;

		static constexpr size_t RAM_QUOTA = 36*1024;

	public:

		Real_gui(Env &env, Session_label const &label)
		:
			_env(env), label(label)
		{ }

		Connection<Gui::Session> connection {
			_env, label, Genode::Ram_quota { RAM_QUOTA }, /* Args */ { } };

		Gui::Session_client session { connection.cap() };

	private:

		Attached_dataspace _command_ds { _env.rm(), session.command_dataspace() };

		Command_buffer &_command_buffer { *_command_ds.local_addr<Command_buffer>() };

	public:

		Gui::View_ids view_ids { };

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

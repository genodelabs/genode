/*
 * \brief  LOG service that prints to a terminal
 * \author Stefan Kalkowski
 * \date   2012-05-21
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <root/component.h>
#include <base/component.h>
#include <base/heap.h>
#include <util/string.h>

#include <terminal_session/connection.h>
#include <log_session/log_session.h>


namespace Genode {

	class Termlog_component : public Rpc_object<Log_session>
	{
		public:

			enum { LABEL_LEN = 64 };

		private:

			char                  _label[LABEL_LEN];
			Terminal::Connection &_terminal;

		public:

			/**
			 * Constructor
			 */
			Termlog_component(const char *label, Terminal::Connection &terminal)
			: _terminal(terminal) { snprintf(_label, LABEL_LEN, "[%s] ", label); }


			/*****************
			 ** Log session **
			 *****************/

			/**
			 * Write a log-message to the terminal.
			 *
			 * The following function's code is a modified variant of the one in:
			 * 'base/src/core/include/log_session_component.h'
			 */
			void write(String const &string_buf) override
			{
				if (!(string_buf.valid_string())) {
					Genode::error("corrupted string");
					return;
				}

				char const *string = string_buf.string();
				int len = strlen(string);

				/*
				 * Heuristic: The Log console implementation flushes
				 *            the output preferably in front of escape
				 *            sequences. If the line contains only
				 *            the escape sequence, we skip the printing
				 *            of the label and cut the line break (last
				 *            character).
				 */
				enum { ESC = 27 };
				if ((string[0] == ESC) && (len == 5) && (string[4] == '\n')) {
					_terminal.write(string, len - 1);
					return;
				}

				_terminal.write(_label, strlen(_label));
				_terminal.write(string, len);

				/* if last character of string was not a line break, add one */
				if ((len > 0) && (string[len - 1] != '\n'))
					_terminal.write("\n", 1);

				/* carriage-return as expected by hardware terminals on newline */
				_terminal.write("\r", 1);
			}
	};


	class Termlog_root : public Root_component<Termlog_component>
	{
		private:

			Terminal::Connection _terminal;

		protected:

			/**
			 * Root component interface
			 */
			Termlog_component *_create_session(const char *args) override
			{
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);

				/* delete ram quota by the memory needed for the session */
				size_t session_size = max((size_t)4096, sizeof(Termlog_component));
				if (ram_quota < session_size)
					throw Insufficient_ram_quota();

				char label_buf[Termlog_component::LABEL_LEN];

				Arg label_arg = Arg_string::find_arg(args, "label");
				label_arg.string(label_buf, sizeof(label_buf), "");

				return new (md_alloc()) Termlog_component(label_buf, _terminal);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  entry point for managing cpu session objects
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Termlog_root(Genode::Env &env, Allocator &md_alloc)
			: Root_component<Termlog_component>(env.ep(), md_alloc),
			  _terminal(env, "log") { }
	};
}


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	static Sliced_heap session_alloc(env.ram(), env.rm());
	static Genode::Termlog_root termlog_root(env, session_alloc);

	env.parent().announce(env.ep().manage(termlog_root));
}

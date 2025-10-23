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

#include <timer_session/connection.h>
#include <terminal_session/connection.h>
#include <log_session/log_session.h>


namespace Terminal_log {

	using namespace Genode;

	struct Main;
}


struct Terminal_log::Main
{
	Env &_env;

	Sliced_heap _session_alloc { _env.ram(), _env.rm() };

	class Component : public Rpc_object<Log_session>
	{
		public:

			enum { LABEL_LEN = 64u };

		private:

			using Label = Genode::String<LABEL_LEN>;
			Label const _label;

			Terminal::Connection &_terminal;
			Timer::Connection    &_timer;

		public:

			Component(const char *label,
			          Terminal::Connection &terminal, Timer::Connection &timer)
			:
				_label("[", label, "] "),
				_terminal(terminal), _timer(timer)
			{ }


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
				size_t len = strlen(string);

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

				uint64_t const ms = _timer.curr_time().trunc_to_plain_ms().value;
				Genode::String<32> const time { ms/1000, ".", (ms/100)%10, " " };

				_terminal.write(time.string(), time.length() - 1);
				_terminal.write(_label.string(), strlen(_label.string()));
				_terminal.write(string, len);

				/* if last character of string was not a line break, add one */
				if ((len > 0) && (string[len - 1] != '\n'))
					_terminal.write("\n", 1);
			}
	};


	class Root : public Root_component<Component>
	{
		private:

			Terminal::Connection _terminal;
			Timer::Connection    _timer;

		protected:

			/**
			 * Root component interface
			 */
			Create_result _create_session(const char *args) override
			{
				char label_buf[Component::LABEL_LEN];

				Arg label_arg = Arg_string::find_arg(args, "label");
				label_arg.string(label_buf, sizeof(label_buf), "");

				return *new (md_alloc()) Component(label_buf, _terminal, _timer);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  entry point for managing cpu session objects
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Root(Genode::Env &env, Allocator &md_alloc)
			:
				Root_component<Component>(env.ep(), md_alloc),
				_terminal(env, "log"), _timer(env)
			{ }

	} _root { _env, _session_alloc };

	Main(Env &env) : _env(env)
	{
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Terminal_log::Main main { env };
}

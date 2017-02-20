/*
 * \brief  Noux child environment
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2012-07-19
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__CHILD_ENV_H_
#define _NOUX__CHILD_ENV_H_

/* Genode includes */
#include <util/string.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include <range_checked_index.h>

namespace Noux {
	template <size_t> class Child_env;
	using namespace Genode;
}


/**
 * \param ARGS_SIZE  size of the argument buffer given
 *                   to the constructor
 */
template <Noux::size_t ARGS_SIZE>
class Noux::Child_env
{
	private:

		enum { MAX_LEN_INTERPRETER_LINE = 128 };

		char const *_binary_name;
		char        _args[ARGS_SIZE + MAX_LEN_INTERPRETER_LINE];
		Sysio::Env  _env;

		void _process_env(Sysio::Env env)
		{
			memcpy(_env, env, sizeof(Sysio::Env));
		}

		/**
		 * Handle the case that the given binary needs an interpreter
		 */
		void _process_binary_name_and_args(Region_map &local_rm,
		                                   const char *binary_name,
		                                   Dataspace_capability binary_ds,
		                                   const char *args)
		{
			bool interpretable = true;

			const size_t binary_size = Dataspace_client(binary_ds).size();

			if (binary_size < 4)
				interpretable = false;

			const char *binary_addr = 0;
			if (interpretable)
				try {
					binary_addr = local_rm.attach(binary_ds);
				} catch(...)  {
					warning("could not attach dataspace");
					interpretable = false;
				}

			if (interpretable &&
			    ((binary_addr[0] != '#') || (binary_addr[1] != '!')))
				interpretable = false;

			if (!interpretable) {
				local_rm.detach(binary_addr);
				_binary_name = binary_name;
				Genode::memcpy(_args, args, ARGS_SIZE);
				return;
			}

			/* find end of line */
			Range_checked_index<unsigned int>
				eol(2, min(binary_size, MAX_LEN_INTERPRETER_LINE));

			try {
				while (binary_addr[eol] != '\n') eol++;
			} catch (Index_out_of_range) { }

			/* skip leading spaces */
			Range_checked_index<unsigned int>
				interpreter_line_cursor(2, eol);

			try {
				while (binary_addr[interpreter_line_cursor] == ' ')
					interpreter_line_cursor++;
			} catch (Index_out_of_range) { }

			/* no interpreter name found */
			if (interpreter_line_cursor == eol)
				throw Child::Binary_does_not_exist();

			int interpreter_name_start = interpreter_line_cursor;

			/* find end of interpreter name */
			try {
				while (binary_addr[interpreter_line_cursor] != ' ')
					interpreter_line_cursor++;
			} catch (Index_out_of_range) { }

			size_t interpreter_name_len =
				interpreter_line_cursor - interpreter_name_start;

			/* copy interpreter name into argument buffer */
			unsigned int args_buf_cursor = 0;
			Genode::strncpy(&_args[args_buf_cursor],
			                &binary_addr[interpreter_name_start],
			                interpreter_name_len + 1);
			_binary_name = &_args[args_buf_cursor];
			args_buf_cursor += interpreter_name_len + 1;

			/* skip more spaces */
			try {
				while (binary_addr[interpreter_line_cursor] == ' ')
					interpreter_line_cursor++;
			} catch (Index_out_of_range) { }

			/* append interpreter arguments to argument buffer */
			size_t interpreter_args_len = eol - interpreter_line_cursor;
			if (interpreter_args_len > 0) {
				Genode::strncpy(&_args[args_buf_cursor],
				                &binary_addr[interpreter_line_cursor],
				                interpreter_args_len + 1);
				args_buf_cursor += interpreter_args_len + 1;
			}

			/* append script arguments to argument buffer */
			Genode::memcpy(&_args[args_buf_cursor],
			               args, ARGS_SIZE);

			local_rm.detach(binary_addr);
		}

	public:

		Child_env(Region_map &local_rm, const char *binary_name,
		          Dataspace_capability binary_ds, const char *args, Sysio::Env env)
		{
			_process_env(env);
			_process_binary_name_and_args(local_rm, binary_name, binary_ds, args);
		}

		char const *binary_name() const { return _binary_name; }

		Args args() { return Args(_args, sizeof(_args)); }

		Sysio::Env const &env() const { return _env; }
};

#endif /* _NOUX__CHILD_ENV_H_ */

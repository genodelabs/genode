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
		 * Verify that the file exists and return its size
		 */
		Vfs::file_size _file_size(Vfs::File_system &root_dir,
		                          char const *binary_name)
		{
			Vfs::Directory_service::Stat stat_out;
			Vfs::Directory_service::Stat_result stat_result;
			
			stat_result = root_dir.stat(binary_name, stat_out);

			switch (stat_result) {
			case Vfs::Directory_service::STAT_OK:
				break;
			case Vfs::Directory_service::STAT_ERR_NO_ENTRY:
				throw Binary_does_not_exist();
			case Vfs::Directory_service::STAT_ERR_NO_PERM:
				throw Binary_is_not_accessible();
			}

			return stat_out.size;
		}

		/**
		 * Verify that the file is a valid ELF executable
		 */
		void _verify_elf(char const *file)
		{
			if ((file[0] != 0x7f) ||
			    (file[1] != 'E') ||
			    (file[2] != 'L') ||
			    (file[3] != 'F'))
			    throw Binary_is_not_executable();
		}

		/**
		 * Handle the case that the given binary needs an interpreter
		 */
		void _process_binary_name_and_args(char const *binary_name,
		                                   char const *args,
		                                   Vfs::File_system &root_dir,
		                                   Vfs_io_waiter_registry &vfs_io_waiter_registry,
		                                   Ram_session &ram,
		                                   Region_map &rm,
		                                   Allocator &alloc)
		{
			Vfs::file_size binary_size = _file_size(root_dir, binary_name);

			if (binary_size == 0)
				throw Binary_is_not_executable();

			/*
			 * We may have to check the dataspace twice because the binary
			 * could be a script that uses an interpreter which might not
			 * exist.
			 */
			Reconstructible<Vfs_dataspace> binary_ds {
				root_dir, vfs_io_waiter_registry,
				binary_name, ram, rm, alloc
			};

			if (!binary_ds->ds.valid())
				throw Binary_is_not_executable();

			Reconstructible<Attached_dataspace> attached_binary_ds(rm, binary_ds->ds);

			char const *binary_addr = attached_binary_ds->local_addr<char const>();

			/* look for '#!' */
			if ((binary_addr[0] != '#') || (binary_addr[1] != '!')) {
				_binary_name = binary_name;
				Genode::memcpy(_args, args, ARGS_SIZE);
				_verify_elf(binary_addr);
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
				throw Binary_does_not_exist();

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

			/* check if interpreter exists and is executable */

			binary_size = _file_size(root_dir, _binary_name);

			if (binary_size == 0)
				throw Binary_is_not_executable();

			binary_ds.construct(root_dir, vfs_io_waiter_registry,
			                    _binary_name, ram,
			                    rm, alloc);

			if (!binary_ds->ds.valid())
				throw Binary_is_not_executable();

			attached_binary_ds.construct(rm, binary_ds->ds);

			_verify_elf(attached_binary_ds->local_addr<char const>());
		}

	public:

		struct Binary_does_not_exist    : Exception { };
		struct Binary_is_not_accessible : Exception { };
		struct Binary_is_not_executable : Exception { };

		Child_env(char const *binary_name,
		          char const *args, Sysio::Env env,
		          Vfs::File_system &root_dir,
		          Vfs_io_waiter_registry &vfs_io_waiter_registry,
		          Ram_session &ram,
		          Region_map &rm,
		          Allocator &alloc)
		{
			_process_env(env);
			_process_binary_name_and_args(binary_name, args, root_dir,
			                              vfs_io_waiter_registry,
			                              ram, rm, alloc);
		}

		char const *binary_name() const { return _binary_name; }

		Args args() { return Args(_args, sizeof(_args)); }

		Sysio::Env const &env() const { return _env; }
};

#endif /* _NOUX__CHILD_ENV_H_ */

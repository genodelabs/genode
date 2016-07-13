/*
 * \brief  Stdio filesystem
 * \author Josef Soentgen
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__STDIO_FILE_SYSTEM_H_
#define _NOUX__STDIO_FILE_SYSTEM_H_

/* Genode includes */
#include <base/log.h>
#include <util/string.h>
#include <vfs/single_file_system.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include "terminal_connection.h"


namespace Noux {

	class Stdio_file_system : public Vfs::Single_file_system
	{
		private:

			Terminal::Session_client *_terminal;
			bool _echo;

		public:

			Stdio_file_system(Xml_node config)
			:
				Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
				_terminal(terminal()),
				_echo(true)
			{ }

			static char const *name() { return "stdio"; }


			/********************************
			 ** File I/O service interface **
			 ********************************/

			Write_result write(Vfs::Vfs_handle *, char const *buf,
			                   Vfs::file_size buf_size,
			                   Vfs::file_size &out_count) override
			{
				buf_size = buf_size > 0xFFFFFFFFULL ? ~0UL : buf_size;

				out_count = _terminal->write(buf, buf_size);

				return WRITE_OK;
			}

			Read_result read(Vfs::Vfs_handle *, char *dst,
			                 Vfs::file_size count,
			                 Vfs::file_size &out_count) override
			{
				count = count > 0xFFFFFFFFULL ? ~0UL : count;

				out_count = _terminal->read(dst, count);

				if (_echo)
					_terminal->write(dst, count);

				return READ_OK;
			}

			Ftruncate_result ftruncate(Vfs::Vfs_handle *,
			                           Vfs::file_size) override
			{
				return FTRUNCATE_OK;
			}

			Ioctl_result ioctl(Vfs::Vfs_handle *vfs_handle, Ioctl_opcode opcode,
			                   Ioctl_arg arg, Ioctl_out &out) override
			{
				switch (opcode) {

				case Vfs::File_io_service::IOCTL_OP_TIOCSETAF:
					{
						_echo = (arg & (Vfs::File_io_service::IOCTL_VAL_ECHO));
						return IOCTL_OK;
					}

				case Vfs::File_io_service::IOCTL_OP_TIOCSETAW:
					{
						warning(__func__, ": OP_TIOCSETAW not implemented");
						return IOCTL_ERR_INVALID;
					}

				default:
					warning(__func__, ": invalid ioctl(request=", Hex(opcode), ")");
					break;
				}

				return IOCTL_ERR_INVALID;
			}
	};
}

#endif /* _NOUX__STDIO_FILE_SYSTEM_H_ */

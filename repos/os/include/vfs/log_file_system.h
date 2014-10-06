/*
 * \brief  LOG file system
 * \author Norman Feske
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__LOG_FILE_SYSTEM_H_
#define _INCLUDE__VFS__LOG_FILE_SYSTEM_H_

#include <log_session/connection.h>
#include <vfs/single_file_system.h>

namespace Vfs { class Log_file_system; }


class Vfs::Log_file_system : public Single_file_system
{
	private:

		Genode::Log_connection _log;

	public:

		Log_file_system(Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config)
		{ }

		static const char *name() { return "log"; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *src, file_size count,
		                   file_size &out_count) override
		{
			out_count = count;

			/* count does not include the trailing '\0' */
			while (count > 0) {
				char tmp[256];
				int const curr_count = min(count, sizeof(tmp) - 1);
				memcpy(tmp, src, curr_count);
				tmp[curr_count > 0 ? curr_count : 0] = 0;
				_log.write(tmp);
				count -= curr_count;
				src   += curr_count;
			}

			return WRITE_OK;
		}

		Read_result read(Vfs_handle *, char *, file_size,
		                file_size &out_count) override
		{
			out_count = 0;
			return READ_OK;
		}
};

#endif /* _INCLUDE__VFS__LOG_FILE_SYSTEM_H_ */

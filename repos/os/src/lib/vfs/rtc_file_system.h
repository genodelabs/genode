/*
 * \brief  Rtc file system
 * \author Josef Soentgen
 * \date   2014-08-20
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__RTC_FILE_SYSTEM_H_
#define _INCLUDE__VFS__RTC_FILE_SYSTEM_H_

/* Genode includes */
#include <rtc_session/connection.h>
#include <vfs/file_system.h>


namespace Vfs { class Rtc_file_system; }


class Vfs::Rtc_file_system : public Single_file_system
{
	private:

		Rtc::Connection _rtc;

	public:

		Rtc_file_system(Genode::Env &env,
		                Genode::Allocator&,
		                Genode::Xml_node config,
		                Io_response_handler &)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_rtc(env)
		{ }

		static char const *name()   { return "rtc"; }
		char const *type() override { return "rtc"; }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.mode |= 0444;

			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *, file_size,
		                   file_size &) override
		{
			return WRITE_ERR_IO;
		}

		/**
		 * Read the current time from the Rtc session
		 *
		 * On each read the current time is queried and afterwards formated
		 * as '%Y-%m-%d %H:%M\n'.
		 */
		Read_result read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                 file_size &out_count) override
		{
			enum { TIMESTAMP_LEN = 17 };

			file_size seek = vfs_handle->seek();

			if (seek >= TIMESTAMP_LEN) {
				out_count = 0;
				return READ_OK;
			}

			Rtc::Timestamp ts = _rtc.current_time();

			char buf[TIMESTAMP_LEN+1];
			char *b = buf;
			unsigned n = Genode::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u\n",
			                              ts.year, ts.month, ts.day, ts.hour, ts.minute);
			n -= seek;
			b += seek;

			file_size len = count > n ? n : count;
			Genode::memcpy(dst, b, len);
			out_count = len;

			return READ_OK;
		}

		bool read_ready(Vfs_handle *) override { return true; }
};

#endif /* _INCLUDE__VFS__RTC_FILE_SYSTEM_H_ */

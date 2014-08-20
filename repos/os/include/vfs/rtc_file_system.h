/*
 * \brief  Rtc file system
 * \author Josef Soentgen
 * \date   2014-08-20
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__RTC_FILE_SYSTEM_H_
#define _INCLUDE__VFS__RTC_FILE_SYSTEM_H_

/* Genode includes */
#include <rtc_session/connection.h>
#include <vfs/file_system.h>

/* libc includes */
#include <time.h>

namespace Vfs { class Rtc_file_system; }


class Vfs::Rtc_file_system : public Single_file_system
{
	private:

		Rtc::Connection _rtc;

	public:

		Rtc_file_system(Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config)
		{ }

		static char const *name() { return "rtc"; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *, size_t count, size_t &count_out) override
		{
			return WRITE_ERR_IO;
		}

		/**
		 * Read the current time from the RTC
		 *
		 * On each read the current time is queried and afterwards formated
		 * as '%Y-%m-%d %H:%M\n'.
		 */
		Read_result read(Vfs_handle *vfs_handle, char *dst, size_t count, size_t &out_count) override
		{
			time_t t = _rtc.get_current_time() / 1000000ULL;

			struct tm *tm = localtime(&t);

			char buf[16 + 1 + 1];
			Genode::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d\n",
			                 1900 + tm->tm_year, /* years since 1900 */
			                 1 + tm->tm_mon,     /* months since January [0-11] */
			                 tm->tm_mday, tm->tm_hour, tm->tm_min);

			size_t len = count > sizeof(buf) ? sizeof(buf) : count;
			Genode::memcpy(dst, buf, len);

			out_count = len;

			return READ_OK;
		}
};

#endif /* _INCLUDE__VFS__RTC_FILE_SYSTEM_H_ */

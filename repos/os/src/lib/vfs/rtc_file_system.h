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
#include <base/registry.h>
#include <rtc_session/connection.h>
#include <vfs/file_system.h>


namespace Vfs { class Rtc_file_system; }


class Vfs::Rtc_file_system : public Single_file_system
{
	private:

		/* "1970-01-01 00:00\n" */
		enum { TIMESTAMP_LEN = 17 };

		class Rtc_vfs_handle : public Single_vfs_handle
		{
			private:

				Rtc::Connection &_rtc;

			public:

				Rtc_vfs_handle(Directory_service &ds,
				               File_io_service   &fs,
				               Genode::Allocator &alloc,
				               Rtc::Connection &rtc)
				: Single_vfs_handle(ds, fs, alloc, 0),
				  _rtc(rtc) { }

				/**
				 * Read the current time from the Rtc session
				 *
				 * On each read the current time is queried and afterwards formated
				 * as '%Y-%m-%d %H:%M\n'.
				 */
				Read_result read(char *dst, file_size count,
				                 file_size &out_count) override
				{
					if (seek() >= TIMESTAMP_LEN) {
						out_count = 0;
						return READ_OK;
					}

					Rtc::Timestamp ts = _rtc.current_time();

					char buf[TIMESTAMP_LEN+1];
					char *b = buf;
					Genode::size_t n = Genode::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u\n",
					                                    ts.year, ts.month, ts.day, ts.hour, ts.minute);
					n -= (size_t)seek();
					b += seek();

					file_size len = count > n ? n : count;
					Genode::memcpy(dst, b, (size_t)len);
					out_count = len;

					return READ_OK;

				}

				Write_result write(char const *, file_size, file_size &) override
				{
					return WRITE_ERR_IO;
				}

				bool read_ready() override { return true; }
		};

		typedef Genode::Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Genode::Registry<Registered_watch_handle> Watch_handle_registry;

		Rtc::Connection _rtc;

		Watch_handle_registry _handle_registry { };

		Genode::Io_signal_handler<Rtc_file_system> _set_signal_handler;

		void _handle_set_signal()
		{
			_handle_registry.for_each([] (Registered_watch_handle &handle) {
				handle.watch_response();
			});
		}

	public:

		Rtc_file_system(Vfs::Env &env, Genode::Xml_node config)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, name(),
			                   Node_rwx::ro(), config),
			_rtc(env.env()),
			_set_signal_handler(env.env().ep(), *this,
			                    &Rtc_file_system::_handle_set_signal)
		{
			_rtc.set_sigh(_set_signal_handler);
		}

		static char const *name()   { return "rtc"; }
		char const *type() override { return "rtc"; }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc)
					Rtc_vfs_handle(*this, *this, alloc, _rtc);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);

			if (result == STAT_OK) {
				out.size = TIMESTAMP_LEN;
			}

			return result;
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				Vfs_watch_handle &watch_handle = *new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				*handle = &watch_handle;
				return WATCH_OK;
			}
			catch (Genode::Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			Genode::destroy(handle->alloc(),
			                static_cast<Registered_watch_handle *>(handle));
		}
};

#endif /* _INCLUDE__VFS__RTC_FILE_SYSTEM_H_ */

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
#include <util/formatted_output.h>
#include <rtc_session/connection.h>
#include <vfs/file_system.h>


namespace Vfs { class Rtc_file_system; }


class Vfs::Rtc_file_system : public Single_file_system
{
	private:

		/* "1970-01-01 00:00:00\n" */
		enum { TIMESTAMP_LEN = 20 };

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
				 * as '%Y-%m-%d %H:%M:%S\n' resp. '%F %T\n'.
				 */
				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					if (seek() >= TIMESTAMP_LEN) {
						out_count = 0;
						return READ_OK;
					}

					Rtc::Timestamp ts = _rtc.current_time();

					struct Padded
					{
						unsigned pad, value;

						void print(Genode::Output &out) const
						{
							using namespace Genode;

							unsigned const len = printed_length(value);
							if (len < pad)
								Genode::print(out, Repeated(pad - len, Char('0')));

							Genode::print(out, value);
						}
					};

					String<TIMESTAMP_LEN+1> string { Padded { 4, ts.year   }, "-",
					                                 Padded { 2, ts.month  }, "-",
					                                 Padded { 2, ts.day    }, " ",
					                                 Padded { 2, ts.hour   }, ":",
					                                 Padded { 2, ts.minute }, ":",
					                                 Padded { 2, ts.second }, "\n" };
					char const *b = string.string();
					size_t      n = string.length();

					n -= size_t(seek());
					b += seek();

					size_t const len = min(n, dst.num_bytes);
					memcpy(dst.start, b, len);
					out_count = len;

					return READ_OK;

				}

				Write_result write(Const_byte_range_ptr const &, size_t &) override
				{
					return WRITE_ERR_IO;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return false; }
		};

		using Registered_watch_handle = Genode::Registered<Vfs_watch_handle>;
		using Watch_handle_registry   = Genode::Registry<Registered_watch_handle>;

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

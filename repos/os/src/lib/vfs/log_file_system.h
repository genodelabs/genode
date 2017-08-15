/*
 * \brief  LOG file system
 * \author Norman Feske
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__LOG_FILE_SYSTEM_H_
#define _INCLUDE__VFS__LOG_FILE_SYSTEM_H_

#include <log_session/connection.h>
#include <vfs/single_file_system.h>
#include <util/reconstructible.h>

namespace Vfs { class Log_file_system; }


class Vfs::Log_file_system : public Single_file_system
{
	private:

		typedef Genode::String<64> Label;
		Label _label;

		Genode::Constructible<Genode::Log_connection>     _log_connection;
		Genode::Constructible<Genode::Log_session_client> _log_client;

		Genode::Log_session & _log_session(Genode::Env &env)
		{
			if (_label.valid()) {
				_log_connection.construct(env, _label);
				return *_log_connection;
			} else {
				_log_client.construct(
					Genode::reinterpret_cap_cast<Genode::Log_session>
					(env.parent().session_cap(Genode::Parent::Env::log())));
				return *_log_client;
			}
		}

		Genode::Log_session &_log;

		class Log_vfs_handle : public Single_vfs_handle
		{
			private:

				Genode::Log_session &_log;

			public:

				Log_vfs_handle(Directory_service &ds,
				               File_io_service   &fs,
				               Genode::Allocator &alloc,
				               Genode::Log_session &log)
				: Single_vfs_handle(ds, fs, alloc, 0),
				  _log(log) { }

				Read_result read(char *dst, file_size count,
				                 file_size &out_count) override
				{
					out_count = 0;
					return READ_OK;
				}

				Write_result write(char const *src, file_size count,
				                   file_size &out_count) override
				{
					out_count = count;

					/* count does not include the trailing '\0' */
					while (count > 0) {
						char tmp[Genode::Log_session::MAX_STRING_LEN];
						int const curr_count = min(count, sizeof(tmp) - 1);
						memcpy(tmp, src, curr_count);
						tmp[curr_count > 0 ? curr_count : 0] = 0;
						_log.write(tmp);
						count -= curr_count;
						src   += curr_count;
					}

					return WRITE_OK;
				}

				bool read_ready() { return false; }
		};

	public:

		Log_file_system(Genode::Env &env,
		                Genode::Allocator&,
		                Genode::Xml_node config,
		                Io_response_handler &)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_label(config.attribute_value("label", Label())),
			_log(_log_session(env))
		{ }

		static const char *name()   { return "log"; }
		char const *type() override { return "log"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle = new (alloc) Log_vfs_handle(*this, *this, alloc,
			                                         _log);
			return OPEN_OK;
		}
};

#endif /* _INCLUDE__VFS__LOG_FILE_SYSTEM_H_ */

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

		using Label = Genode::String<64>;
		Label _label;

		Genode::Constructible<Genode::Log_connection>     _log_connection { };
		Genode::Constructible<Genode::Log_session_client> _log_client     { };

		Genode::Log_session & _log_session(Genode::Env &env)
		{
			using namespace Genode;

			if (_label.valid()) {
				_log_connection.construct(env, _label);
				return *_log_connection;
			}

			_log_client.construct(
				env.parent().session_cap(Parent::Env::log())
					.convert<Capability<Log_session>>(
						[&] (Capability<Session> cap) {
							return static_cap_cast<Log_session>(cap); },
						[&] (Parent::Session_cap_error) {
							return Capability<Log_session>(); }));
			return *_log_client;
		}

		Genode::Log_session &_log;

		class Log_vfs_handle : public Single_vfs_handle
		{
			private:

				char _line_buf[Genode::Log_session::MAX_STRING_LEN];

				size_t _line_pos = 0;

				Genode::Log_session &_log;

				void _flush()
				{
					file_offset strip = 0;
					for (file_offset i = _line_pos - 1; i > 0; --i) {
						switch(_line_buf[i]) {
						case '\n':
						case '\t':
						case ' ':
							++strip;
							--_line_pos;
							continue;
						}
						break;
					}

					_line_buf[_line_pos > 0 ? _line_pos : 0] = '\0';

					_log.write(_line_buf);
					_line_pos = 0;
				}

			public:

				Log_vfs_handle(Directory_service &ds,
				               File_io_service   &fs,
				               Genode::Allocator &alloc,
				               Genode::Log_session &log)
				:
					Single_vfs_handle(ds, fs, alloc, 0), _log(log)
				{ }

				~Log_vfs_handle()
				{
					if (_line_pos > 0) _flush();
				}

				Read_result read(Byte_range_ptr const &, size_t &) override
				{
					/* block indefinitely - mimics stdout resp. stdin w/o input */
					return READ_QUEUED;
				}

				Write_result write(Const_byte_range_ptr const &buf, size_t &out_count) override
				{
					size_t       count = buf.num_bytes;
					char const * src   = buf.start;

					out_count = count;

					/* count does not include the trailing '\0' */
					while (count > 0) {
						size_t curr_count = min(count, sizeof(_line_buf) - 1 - _line_pos);

						for (size_t i = 0; i < curr_count; ++i) {
							if (src[i] == '\n') {
								curr_count = i + 1;
								break;
							}
						}

						memcpy(_line_buf + _line_pos, src, curr_count);
						_line_pos += curr_count;

						if ((_line_pos == sizeof(_line_buf) - 1) ||
						    (_line_buf[_line_pos - 1] == '\n'))
							_flush();

						count -= curr_count;
						src   += curr_count;
					}
					return WRITE_OK;
				}

				bool read_ready()  const override { return false; }
				bool write_ready() const override { return true; }

				Sync_result sync() override
				{
					if (_line_pos > 0)
						_flush();

					return SYNC_OK;
				}
		};

	public:

		Log_file_system(Vfs::Env &env, Genode::Xml_node const &config)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, name(),
			                   Node_rwx::wo(), config),
			_label(config.attribute_value("label", Label())),
			_log(_log_session(env.env()))
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

			try {
				*out_handle = new (alloc)
					Log_vfs_handle(*this, *this, alloc, _log);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}


		/*******************************
		 ** File_io_service interface **
		 *******************************/

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			/*
			 * Return success to allow for output redirection via '> /dev/log'.
			 * The shell call ftruncate after opening the destination file.
			 */
			return FTRUNCATE_OK;
		}
};

#endif /* _INCLUDE__VFS__LOG_FILE_SYSTEM_H_ */

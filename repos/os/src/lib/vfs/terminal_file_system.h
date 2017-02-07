/*
 * \brief  Terminal file system
 * \author Christian Prochaska
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_
#define _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_

#include <terminal_session/connection.h>
#include <vfs/single_file_system.h>
#include <base/signal.h>


namespace Vfs { class Terminal_file_system; }


class Vfs::Terminal_file_system : public Single_file_system
{
	private:

		typedef Genode::String<64> Label;
		Label _label;

		Genode::Env         &_env;
		Io_response_handler &_io_handler;

		Terminal::Connection _terminal { _env, _label.string() };

		Genode::Signal_handler<Terminal_file_system> _read_avail_handler {
			_env.ep(), *this, &Terminal_file_system::_handle_read_avail };

		void _handle_read_avail() { _io_handler.handle_io_response(); }

		Read_result _read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                  file_size &out_count)
		{
			if (_terminal.avail()) {
				out_count = _terminal.read(dst, count);
				return READ_OK;
			} else {
				return READ_QUEUED;
			}
		}

	public:

		Terminal_file_system(Genode::Env &env,
		                     Genode::Allocator&,
		                     Genode::Xml_node config,
		                     Io_response_handler &io_handler)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_label(config.attribute_value("label", Label())),
			_env(env), _io_handler(io_handler)
		{
			/* register for read-avail notification */
			_terminal.read_avail_sigh(_read_avail_handler);
		}

		static const char *name()   { return "terminal"; }
		char const *type() override { return "terminal"; }

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *buf, file_size buf_size,
		                   file_size &out_count) override
		{
			out_count = _terminal.write(buf, buf_size);
			return WRITE_OK;
		}

		Read_result read(Vfs_handle *, char *dst, file_size count,
		                 file_size &out_count) override
		{
			out_count = _terminal.read(dst, count);
			return READ_OK;
		}

		bool queue_read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                Read_result &out_result, file_size &out_count) override
		{
			out_result = _read(vfs_handle, dst, count, out_count);
			return true;
		}

		Read_result complete_read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                          file_size &out_count) override
		{
			return _read(vfs_handle, dst, count, out_count);
		}

		bool read_ready(Vfs_handle *) override
		{
			return _terminal.avail();
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			return FTRUNCATE_OK;
		}

		bool check_unblock(Vfs_handle *vfs_handle, bool rd, bool wr, bool ex) override
		{
			if (rd && (_terminal.avail() > 0))
				return true;

			if (wr)
				return true;

			return false;
		}

		void register_read_ready_sigh(Vfs_handle *vfs_handle,
		                              Signal_context_capability sigh) override
		{
			_terminal.read_avail_sigh(sigh);
		}
};

#endif /* _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_ */

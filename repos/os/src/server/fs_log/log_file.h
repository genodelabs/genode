/*
 * \brief  File object shared between log sessions
 * \author Emery Hemingway
 * \date   2015-06-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FS_LOG__LOG_FILE_H_
#define _FS_LOG__LOG_FILE_H_

/* Genode includes */
#include <log_session/log_session.h>
#include <file_system_session/file_system_session.h>

namespace Fs_log {

	using namespace Genode;
	using namespace File_system;

	class Log_file;

}

class Fs_log::Log_file : public List<Log_file>::Element
{
	private:

		char                  _dir_path[ MAX_PATH_LEN];
		char                  _file_name[MAX_NAME_LEN];
		File_system::Session &_fs;
		File_handle           _handle;
		seek_off_t            _offset;
		int                   _clients;

	public:

		/**
		 * Constructor
		 */
		Log_file(File_system::Session &fs, File_handle handle,
		         char const *dir_path, char const *file_name,
		         seek_off_t offset)
		:
			_fs(fs), _handle(handle), _offset(offset), _clients(0)
		{
			strncpy(_dir_path,   dir_path,  sizeof(_dir_path));
			strncpy(_file_name, file_name, sizeof(_file_name));
		}

		~Log_file() { _fs.close(_handle); }

		bool match(char const *dir, char const *filename) const
		{
			return
				(strcmp(_dir_path,  dir,      MAX_PATH_LEN) == 0) &&
				(strcmp(_file_name, filename, MAX_NAME_LEN) == 0);
		}

		void incr() { ++_clients; }
		void decr() { --_clients; }
		int client_count() const { return _clients; }

		/**
		 * Write a log message to the packet buffer.
		 */
		size_t write(char const *msg, size_t msg_len)
		{
			File_system::Session::Tx::Source &source = *_fs.tx();

			File_system::Packet_descriptor raw_packet;
			if (!source.ready_to_submit())
				raw_packet = source.get_acked_packet();
			else
				raw_packet = source.alloc_packet(Log_session::String::MAX_SIZE);

			File_system::Packet_descriptor
				packet(raw_packet,
				       _handle, File_system::Packet_descriptor::WRITE,
				       msg_len, _offset);

			_offset += msg_len;

			char *buf = source.packet_content(packet);
			memcpy(buf, msg, msg_len);

			source.submit_packet(packet);
			return msg_len;
		}
};

#endif

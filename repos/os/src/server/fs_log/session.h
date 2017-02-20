/*
 * \brief  Log session that writes messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-16
 *
 * Message writing is fire-and-forget to prevent
 * logging from becoming I/O bound.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FS_LOG__SESSION_H_
#define _FS_LOG__SESSION_H_

/* Genode includes */
#include <log_session/log_session.h>
#include <file_system_session/file_system_session.h>
#include <base/rpc_server.h>
#include <base/snprintf.h>
#include <base/log.h>

namespace Fs_log {

	enum { MAX_LABEL_LEN = 128 };

	class Session_component;
}

class Fs_log::Session_component : public Genode::Rpc_object<Genode::Log_session>
{
	private:

		char _label_buf[MAX_LABEL_LEN];
		Genode::size_t const _label_len;

		File_system::Session          &_fs;
		File_system::File_handle const _handle;

	public:

		Session_component(File_system::Session     &fs,
		                  File_system::File_handle  handle,
		                  char               const *label)
		:
			_label_len(Genode::strlen(label) ? Genode::strlen(label)+3 : 0),
			_fs(fs), _handle(handle)
		{
			if (_label_len)
				Genode::snprintf(_label_buf, MAX_LABEL_LEN, "[%s] ", label);
		}

		~Session_component()
		{
			_fs.sync(_handle);
			_fs.close(_handle);
		}


		/*****************
		 ** Log session **
		 *****************/

		Genode::size_t write(Log_session::String const &msg)
		{
			using namespace Genode;

			if (!msg.is_valid_string()) {
				Genode::error("received corrupted string");
				return 0;
			}

			size_t msg_len = strlen(msg.string());

			File_system::Session::Tx::Source &source = *_fs.tx();

			File_system::Packet_descriptor packet(
				source.get_acked_packet(),
				_handle, File_system::Packet_descriptor::WRITE,
				msg_len, File_system::SEEK_TAIL);

			char *buf = source.packet_content(packet);

			if (_label_len) {
				memcpy(buf, _label_buf, _label_len);

				if (_label_len+msg_len > Log_session::String::MAX_SIZE) {
					packet.length(msg_len);
					source.submit_packet(packet);

					packet =  File_system::Packet_descriptor(
						source.get_acked_packet(),
				       _handle, File_system::Packet_descriptor::WRITE,
				       msg_len, File_system::SEEK_TAIL);

					buf = source.packet_content(packet);

				} else {
					buf += _label_len;
					packet.length(_label_len+msg_len);
				}
			}

			memcpy(buf, msg.string(), msg_len);

			source.submit_packet(packet);
			return msg_len;
		}
};

#endif

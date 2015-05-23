/*
 * \brief  Log session that writes messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-16
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FS_LOG__SESSION_H_
#define _FS_LOG__SESSION_H_

/* Genode includes */
#include <log_session/log_session.h>
#include <file_system_session/file_system_session.h>
#include <base/rpc_server.h>

namespace Fs_log {

	using namespace Genode;

	class  Session_component;

}

/**
 * A log session that writes messages to a file system node.
 *
 * Message writing is fire-and-forget to prevent
 * logging from becoming I/O bound.
 */
class Fs_log::Session_component : public Rpc_object<Log_session, Session_component>
{
	private:

		File_system::Session    &_fs;
		File_system::File_handle _file_handle;
		File_system::seek_off_t  _offset;

	public:

		/**
		 * Constructor
		 */
		Session_component(File_system::Session    &fs,
		                  File_system::File_handle fh,
		                  File_system::seek_off_t  offset)
		: _fs(fs), _file_handle(fh), _offset(offset) { }

		/*****************
		 ** Log session **
		 *****************/

		size_t write(Log_session::String const &string)
		{
			if (!(string.is_valid_string())) {
				PERR("corrupted string");
				return 0;
			}

			File_system::Session::Tx::Source &source = *_fs.tx();

			char const *msg  = string.string();
			size_t msg_len   = Genode::strlen(msg);
			size_t write_len = msg_len;

			/*
			 * If the message did not fill the incoming buffer
			 * make space to add a newline.
			 */
			if ((msg_len < Log_session::String::MAX_SIZE) &&
			    (msg[msg_len-1] != '\n'))
				++write_len;

			while (source.ack_avail())
				source.release_packet(source.get_acked_packet());

			File_system::Packet_descriptor
				packet(source.alloc_packet(Log_session::String::MAX_SIZE),
				       0, /* The result struct. */
				       _file_handle, File_system::Packet_descriptor::WRITE,
				       write_len, _offset);
			_offset += write_len;

			char *buf = source.packet_content(packet);
			memcpy(buf, msg, msg_len);

			if (msg_len != write_len)
				buf[msg_len] = '\n';

			source.submit_packet(packet);
			return msg_len;
		}
};

#endif

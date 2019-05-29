/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \author Pirmin Duss
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SSH_TERMINAL_TERMINAL_H_
#define _SSH_TERMINAL_TERMINAL_H_

/* Genode includes */
#include <base/capability.h>
#include <base/signal.h>
#include <session/session.h>
#include <base/log.h>
#include <terminal_session/terminal_session.h>

/* local includes */
#include "login.h"


namespace Ssh
{
	using namespace Genode;

	class Terminal;
}


class Ssh::Terminal
{
	public:

		Util::Buffer<4096u> read_buf { };

		int write_avail_fd { -1 };

	private:

		Util::Buffer<4096u> _write_buf { };

		::Terminal::Session::Size _size { 0, 0 };

		Signal_context_capability _size_changed_sigh;
		Signal_context_capability _connected_sigh;
		Signal_context_capability _read_avail_sigh;

		Ssh::User const _user { };

		unsigned _attached_channels { 0u };
		unsigned _pending_channels  { 0u };

	public:

		/**
		 * Constructor
		 */
		Terminal(Ssh::User const &user) : _user(user) { }

		virtual ~Terminal() = default;

		Ssh::User const &user() const { return _user; }

		unsigned attached_channels() const { return _attached_channels; }

		void attach_channel() { ++_attached_channels; }
		void detach_channel() { --_attached_channels; }
		void reset_pending()  { _pending_channels = 0; }

		/*********************************
		 ** Terminal::Session interface **
		 *********************************/

		/**
		 * Register signal handler to be notified once the size was changed
		 */
		void size_changed_sigh(Signal_context_capability sigh) {
			_size_changed_sigh = sigh; }

		/**
		 * Register signal handler to be notified once we accepted the TCP
		 * connection
		 */
		void connected_sigh(Signal_context_capability sigh)
		{
			_connected_sigh = sigh;

			if (_attached_channels > 0) {
				notify_connected();
			}
		}

		/**
		 * Register signal handler to be notified when data is available for
		 * reading
		 */
		void read_avail_sigh(Signal_context_capability sigh)
		{
			_read_avail_sigh = sigh;

			/* if read data is available right now, deliver signal immediately */
			if (read_buffer_empty() && _read_avail_sigh.valid()) {
				Signal_transmitter(_read_avail_sigh).submit();
			}
		}

		/**
		 * Inform client about the finished initialization of the SSH
		 * session
		 */
		void notify_connected()
		{
			if (!_connected_sigh.valid()) { return; }
			Signal_transmitter(_connected_sigh).submit();
		}

		/**
		 * Inform client about avail data
		 */
		void notify_read_avail()
		{
			if (!_read_avail_sigh.valid()) { return; }
			Signal_transmitter(_read_avail_sigh).submit();
		}

		/**
		 * Inform client about the changed size of the remote terminal
		 */
		void notify_size_changed()
		{
			if (!_size_changed_sigh.valid()) { return; }
			Signal_transmitter(_size_changed_sigh).submit();
		}

		/**
		 * Set size of the Terminal session to match remote terminal
		 */
		void size(::Terminal::Session::Size size) { _size = size; }

		/**
		 * Return size of the Terminal session
		 */
		::Terminal::Session::Size size() const { return _size; }

		/*****************
		 ** I/O methods **
		 *****************/

		/**
		 * Send internal write buffer content to SSH channel
		 */
		void send(ssh_channel channel)
		{
			Lock::Guard g(_write_buf.lock());

			if (!_write_buf.read_avail()) { return; }

			/* ignore send request */
			if (!channel || !ssh_channel_is_open(channel)) { return; }

			char const *src     = _write_buf.content();
			size_t const len    = _write_buf.read_avail();
			/* XXX we do not handle partial writes */
			int const num_bytes = ssh_channel_write(channel, src, len);

			if (num_bytes && (size_t)num_bytes < len) {
				warning("send on channel was truncated");
			}

			if (++_pending_channels >= _attached_channels) {
				_write_buf.reset();
			}

			/* at this point the client might have disconnected */
			if (num_bytes < 0) { throw -1; }
		}

		/******************************************
		 ** Methods called by Terminal front end **
		 ******************************************/

		/**
		 * Read out internal read buffer and copy into destination buffer.
		 */
		size_t read(char *dst, size_t dst_len)
		{
			Genode::Lock::Guard g(read_buf.lock());

			size_t const num_bytes = min(dst_len, read_buf.read_avail());
			Genode::memcpy(dst, read_buf.content(), num_bytes);
			read_buf.consume(num_bytes);

			/* notify client if there are still bytes available for reading */
			if (!read_buf.read_avail()) { read_buf.reset(); }
			else {
				if (_read_avail_sigh.valid()) {
					Signal_transmitter(_read_avail_sigh).submit();
				}
			}

			return num_bytes;
		}

		/**
		 * Write into internal buffer and copy to underlying socket
		 */
		size_t write(char const *src, Genode::size_t src_len)
		{
			Lock::Guard g(_write_buf.lock());

			size_t num_bytes = 0;
			size_t i = 0;
			Libc::with_libc([&] {
				while (_write_buf.write_avail() > 0 && i < src_len) {

					char c = src[i];
					if (c == '\n') {
						_write_buf.append('\r');
					}

					_write_buf.append(c);
					num_bytes++;

					i++;
				}
			});

			/* wake the event loop up */
			Libc::with_libc([&] {
				char c = 1;
				::write(write_avail_fd, &c, sizeof(c));
			});

			return num_bytes;
		}

		/**
		 * Return true if the internal read buffer is ready to receive data
		 */
		bool read_buffer_empty()
		{
			Genode::Lock::Guard g(read_buf.lock());
			return !read_buf.read_avail();
		}
};

#endif  /* _SSH_TERMINAL_TERMINAL_H_ */

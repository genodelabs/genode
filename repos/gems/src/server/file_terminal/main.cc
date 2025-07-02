/*
 * \brief  Service providing the 'Terminal_session' interface for a file
 * \author Josef Soentgen
 * \date   2013-10-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <os/session_policy.h>
#include <libc/component.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>

/* libc includes */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <stdio.h>
#include <fcntl.h>
#pragma GCC diagnostic pop  /* restore -Wconversion warnings */

namespace Terminal {
	class  Open_file;
	class  Session_component;
	class  Root_component;
	struct Main;
}


class Terminal::Open_file
{
	private:

		/**
		 * File descriptor for open file
		 */
		int _fd;

		/**
		 * Signal handler to be informed about the established connection
		 */
		Signal_context_capability _connected_sigh { };

		/**
		 * Signal handler to be informed about data available to read
		 */
		Signal_context_capability _read_avail_sigh { };

		/**
		 * Buffer for incoming data
		 *
		 * This buffer is used for synchronizing the reception of data in the
		 * main thread ('watch_sockets_for_incoming_data') and the entrypoint
		 * thread ('read'). The main thread fills the buffer if its not already
		 * occupied and the entrypoint thread consumes the buffer. When the
		 * buffer becomes occupied, the corresponding socket gets removed from
		 * the 'rfds' set of 'select()' until a read request from the terminal
		 * client comes in.
		 */
		enum { READ_BUF_SIZE = 4096 };
		char   _read_buf[READ_BUF_SIZE];
		size_t _read_buf_bytes_used { };

	public:

		Open_file(const char *filename) : _fd(-1)
		{
			Libc::with_libc([&] () { _fd = ::open(filename, O_CREAT|O_RDWR); });
			if (_fd == -1)
				::perror("open");
		}

		~Open_file() { close(_fd); }

		/**
		 * Return file descriptor of the open file
		 */
		int fd() const { return _fd; }

		/**
		 * Register signal handler to be notified once we openend the file
		 */
		void connected_sigh(Signal_context_capability sigh)
		{
			_connected_sigh = sigh;

			/* notify the client immediatly if we have already openend the file */
			if (file_opened() && _connected_sigh.valid())
				Signal_transmitter(_connected_sigh).submit();
		}

		/**
		 * Register signal handler to be notified when data is available for
		 * reading
		 */
		void read_avail_sigh(Signal_context_capability sigh)
		{
			_read_avail_sigh = sigh;

			/* if read data is available right now, deliver signal immediately */
			if (!read_buffer_empty() && _read_avail_sigh.valid())
				Signal_transmitter(_read_avail_sigh).submit();
		}

		/**
		 * Return true if the file was successfully openend
		 */
		bool file_opened() const { return _fd != -1; }

		/**
		 * Fetch data from socket into internal read buffer
		 */
		void fill_read_buffer_and_notify_client()
		{
			if (_read_buf_bytes_used) {
				warning("read buffer already in use");
				return;
			}

			/* read from file */
			_read_buf_bytes_used = ::read(_fd, _read_buf, sizeof(_read_buf));

			/* notify client about bytes available for reading */
			if (_read_avail_sigh.valid())
				Signal_transmitter(_read_avail_sigh).submit();
		}

		/**
		 * Flush internal read buffer into destination buffer
		 */
		size_t flush_read_buffer(char *dst, size_t dst_len)
		{
			size_t num_bytes = min(dst_len, _read_buf_bytes_used);
			memcpy(dst, _read_buf, num_bytes);
			_read_buf_bytes_used = 0;
			return num_bytes;
		}

		/**
		 * Return true if the internal read buffer is ready to receive data
		 */
		bool read_buffer_empty() const { return _read_buf_bytes_used == 0; }
};


class Terminal::Session_component : public Rpc_object<Session, Session_component>,
                                    private Open_file
{
	private:

		Attached_ram_dataspace _io_buffer;

	public:

		Session_component(Env &env,
		                  size_t io_buffer_size, const char *filename)
		:
			Open_file(filename),
			_io_buffer(env.ram(), env.rm(), io_buffer_size)
		{ }

		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() override { return Size(0, 0); }

		bool avail() override
		{
			return !read_buffer_empty();
		}

		size_t _read(size_t dst_len)
		{
			size_t num_bytes =
				flush_read_buffer(_io_buffer.local_addr<char>(),
				                  min(_io_buffer.size(), dst_len));

			return num_bytes;
		}

		size_t _write(size_t num_bytes)
		{
			/* sanitize argument */
			num_bytes = min(num_bytes, _io_buffer.size());

			ssize_t written_bytes = 0;
			Libc::with_libc([&] () {
				/* write data to descriptor */
				written_bytes = ::write(fd(),
				                        _io_buffer.local_addr<char>(),
				                        num_bytes);
			});

			if (written_bytes < 0) {
				error("write error, dropping data");
				return 0;
			}

			return written_bytes;
		}

		Dataspace_capability _dataspace()
		{
			return _io_buffer.cap();
		}

		void read_avail_sigh(Signal_context_capability sigh) override
		{
			Open_file::read_avail_sigh(sigh);
		}

		void connected_sigh(Signal_context_capability sigh) override
		{
			Open_file::connected_sigh(sigh);
		}

		void size_changed_sigh(Signal_context_capability) override { }

		size_t read(void *, size_t) override { return 0; }
		size_t write(void const *, size_t) override { return 0; }
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Env &_env;

		Attached_rom_dataspace const &_config_rom;

	protected:

		Create_result _create_session(const char *args) override
		{
			return with_matching_policy(label_from_args(args), _config_rom.node(),
				[&] (Node const &policy) -> Create_result {
					if (!policy.has_attribute("filename")) {
						error("missing \"filename\" attribute in policy definition");
						return Create_error::DENIED;
					}
					using File_name = String<256>;
					return _alloc_obj(_env,
					                  policy.attribute_value("io_buffer_size", 4096UL),
					                  policy.attribute_value("filename", File_name()).string());
				},
				[&] () -> Create_result { return Create_error::DENIED; });
		}

	public:

		/**
		 * Constructor
		 */
		Root_component(Env &env, Allocator &md_alloc,
		               Attached_rom_dataspace const &config_rom)
		:
			Genode::Root_component<Session_component>(env.ep(), md_alloc),
			_env(env), _config_rom(config_rom)
		{ }
};


struct Terminal::Main
{
	Env &_env;

	Attached_rom_dataspace _config_rom { _env, "config" };

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	/* create root interface for service */
	Terminal::Root_component _root { _env, _sliced_heap, _config_rom };

	Main(Env &env) : _env(env)
	{
		/* announce service at our parent */
		_env.parent().announce(env.ep().manage(_root));
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] () { static Terminal::Main main(env); });
}

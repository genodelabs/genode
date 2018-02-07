/*
 * \brief  Service providing the 'Terminal_session' interface via TCP
 * \author Norman Feske
 * \date   2011-09-07
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/list.h>
#include <util/misc_math.h>
#include <base/log.h>
#include <base/rpc_server.h>
#include <base/heap.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <os/session_policy.h>

#include <libc/component.h>
#include <pthread.h>

/* socket API */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

static bool const verbose = true;


class Open_socket : public Genode::List<Open_socket>::Element
{
	private:

		/**
		 * Socket descriptor for listening to a new TCP connection
		 */
		int _listen_sd;

		/**
		 * Socket descriptor for open TCP connection
		 */
		int _sd;

		/**
		 * Signal handler to be informed about the established connection
		 */
		Genode::Signal_context_capability _connected_sigh;

		/**
		 * Signal handler to be informed about data available to read
		 */
		Genode::Signal_context_capability _read_avail_sigh;

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
		char           _read_buf[READ_BUF_SIZE];
		Genode::size_t _read_buf_bytes_used;
		Genode::size_t _read_buf_bytes_read;

		/**
		 * Establish remote connection
		 *
		 * \return socket descriptor used for the remote TCP connection
		 */
		static int _remote_listen(int tcp_port)
		{
			int listen_sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (listen_sd == -1) {
				Genode::error("socket creation failed");
				return -1;
			}

			sockaddr_in sockaddr;
			sockaddr.sin_family = PF_INET;
			sockaddr.sin_port = htons (tcp_port);
			sockaddr.sin_addr.s_addr = INADDR_ANY;

			if (bind(listen_sd, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
				Genode::error("bind to port ", tcp_port, " failed");
				return -1;
			}

			if (listen(listen_sd, 1)) {
				Genode::error("listen failed");
				return -1;
			}

			Genode::log("listening on port ", tcp_port, "...");
			return listen_sd;
		}

	public:

		Open_socket(int tcp_port);

		~Open_socket();

		/**
		 * Return socket descriptor for listening to new connections
		 */
		int listen_sd() const { return _listen_sd; }

		/**
		 * Return true if all steps of '_remote_listen' succeeded
		 */
		bool listen_sd_valid() const { return _listen_sd != -1; }

		/**
		 * Return socket descriptor of the connection
		 */
		int sd() const { return _sd; }

		/**
		 * Register signal handler to be notified once we accepted the TCP
		 * connection
		 */
		void connected_sigh(Genode::Signal_context_capability sigh) { _connected_sigh = sigh; }

		/**
		 * Register signal handler to be notified when data is available for
		 * reading
		 */
		void read_avail_sigh(Genode::Signal_context_capability sigh)
		{
			_read_avail_sigh = sigh;

			/* if read data is available right now, deliver signal immediately */
			if (!read_buffer_empty() && _read_avail_sigh.valid())
				Genode::Signal_transmitter(_read_avail_sigh).submit();
		}

		/**
		 * Accept new connection, defining the connection's socket descriptor
		 *
		 * This function is called by the 'select()' thread when a new
		 * connection is pending.
		 */
		void accept_remote_connection()
		{
			struct sockaddr addr;
			socklen_t len = sizeof(addr);
			_sd = accept(_listen_sd, &addr, &len);

			if (_sd != -1)
				Genode::log("connection established");

			/*
			 * Inform client about the finished initialization of the terminal
			 * session
			 */
			if (_connected_sigh.valid())
				Genode::Signal_transmitter(_connected_sigh).submit();
		}

		/**
		 * Return true if TCP connection is established
		 *
		 * If the return value is false, we are still in listening more
		 * and have not yet called 'accept()'.
		 */
		bool connection_established() const { return _sd != -1; }

		/**
		 * Fetch data from socket into internal read buffer
		 */
		void fill_read_buffer_and_notify_client()
		{
			if (_read_buf_bytes_used) {
				Genode::warning("read buffer already in use");
				return;
			}

			/* read from socket */
			_read_buf_bytes_used = ::read(_sd, _read_buf, sizeof(_read_buf));

			if (_read_buf_bytes_used == 0) {
				close(_sd);
				_sd = -1;
				return;
			}

			/* notify client about bytes available for reading */
			if (_read_avail_sigh.valid())
				Genode::Signal_transmitter(_read_avail_sigh).submit();
		}

		/**
		 * Read out internal read buffer and copy into destination buffer.
		 */
		Genode::size_t read_buffer(char *dst, Genode::size_t dst_len)
		{
			Genode::size_t num_bytes = Genode::min(dst_len, _read_buf_bytes_used -
			                                       _read_buf_bytes_read);
			Genode::memcpy(dst, _read_buf + _read_buf_bytes_read, num_bytes);

			_read_buf_bytes_read += num_bytes;
			if (_read_buf_bytes_read >= _read_buf_bytes_used)
				_read_buf_bytes_used = _read_buf_bytes_read = 0;

			/* notify client if there are still bytes available for reading */
			if (_read_avail_sigh.valid() && !read_buffer_empty())
				Genode::Signal_transmitter(_read_avail_sigh).submit();

			return num_bytes;
		}

		/**
		 * Return true if the internal read buffer is ready to receive data
		 */
		bool read_buffer_empty() const { return _read_buf_bytes_used == 0; }
};


class Open_socket_pool
{
	private:

		/**
		 * Protection for '_list'
		 */
		Genode::Lock _lock;

		/**
		 * List of open sockets
		 */
		Genode::List<Open_socket> _list;

		/**
		 * Thread doing the select
		 */
		pthread_t _select;

		/**
		 * Number of currently open sockets
		 */
		int _count;

		/**
		 * Pipe used to synchronize 'select()' loop with the entrypoint thread
		 */
		int sync_pipe_fds[2];

		/**
		 * Intercept the blocking state of the current 'select()' call
		 */
		void _wakeup_select()
		{
			char c = 0;
			::write(sync_pipe_fds[1], &c, sizeof(c));
		}

		static void * entry(void *arg) {
			Open_socket_pool * pool = reinterpret_cast<Open_socket_pool *>(arg);

			for (;;)
				pool->watch_sockets_for_incoming_data();

			return nullptr;
		}

	public:

		Open_socket_pool(Genode::Env &env)
		: _count(0)
		{
			pipe(sync_pipe_fds);

			if (pthread_create(&_select, nullptr, entry, this)) {
				class Startup_select_thread_failed : Genode::Exception { };
				throw Startup_select_thread_failed();
			}
		}

		void insert(Open_socket *s)
		{
			Genode::Lock::Guard guard(_lock);
			_list.insert(s);
			_count++;
			_wakeup_select();
		}

		void remove(Open_socket *s)
		{
			Genode::Lock::Guard guard(_lock);
			_list.remove(s);
			_count--;
			_wakeup_select();
		}

		void update_sockets_to_watch()
		{
			_wakeup_select();
		}

		void watch_sockets_for_incoming_data()
		{
			/* prepare arguments for 'select()' */
			fd_set rfds;
			int    nfds = 0;
			{
				Genode::Lock::Guard guard(_lock);

				/* collect file descriptors of all open sessions */
				FD_ZERO(&rfds);
				for (Open_socket *s = _list.first(); s; s = s->next()) {

					/*
					 * If one of the steps of creating the listen socket
					 * failed, skip the session.
					 */
					if (!s->listen_sd_valid())
						continue;

					/*
					 * If the connection is not already established, tell
					 * 'select' to notify us about a new connection.
					 */
					if (!s->connection_established()) {
						nfds = Genode::max(nfds, s->listen_sd());
						FD_SET(s->listen_sd(), &rfds);
						continue;
					}

					/*
					 * The connection is established. We watch the connection's
					 * file descriptor for reading, but only if our buffer can
					 * take new data. Otherwise, we let the incoming data queue
					 * up in the TCP/IP stack.
					 */
					nfds = Genode::max(nfds, s->sd());
					if (s->read_buffer_empty())
						FD_SET(s->sd(), &rfds);
				}

				/* add sync pipe to set of file descriptors to watch */
				FD_SET(sync_pipe_fds[0], &rfds);
				nfds = Genode::max(nfds, sync_pipe_fds[0]);
			}

			/* block for incoming data or sync-pipe events */
			select(nfds + 1, &rfds, NULL, NULL, NULL);

			/* check if we were woken up via the sync pipe */
			if (FD_ISSET(sync_pipe_fds[0], &rfds)) {
				char c = 0;
				::read(sync_pipe_fds[0], &c, 1);
				return;
			}

			/* read pending data from sockets */
			{
				Genode::Lock::Guard guard(_lock);

				for (Open_socket *s = _list.first(); s; s = s->next()) {

					/* look for new connection */
					if (!s->connection_established()) {
						if (FD_ISSET(s->listen_sd(), &rfds))
							s->accept_remote_connection();
						continue;
					}

					/* connection is established, look for incoming data */
					if (FD_ISSET(s->sd(), &rfds))
						s->fill_read_buffer_and_notify_client();
				}
			}
		}
};


Open_socket_pool *open_socket_pool(Genode::Env * env = nullptr)
{
	static Open_socket_pool inst(*env);
	return &inst;
}


Open_socket::Open_socket(int tcp_port)
:
	_listen_sd(_remote_listen(tcp_port)), _sd(-1),
	_read_buf_bytes_used(0), _read_buf_bytes_read(0)
{
	open_socket_pool()->insert(this);
}


Open_socket::~Open_socket()
{
	if (_sd != -1) close(_sd);
	open_socket_pool()->remove(this);
}


namespace Terminal {
	class Session_component;
	class Root_component;
};

class Terminal::Session_component : public Genode::Rpc_object<Session, Session_component>,
                                    public Open_socket
{
	private:

		Genode::Attached_ram_dataspace _io_buffer;

	public:

		Session_component(Genode::Env &env, Genode::size_t io_buffer_size, int tcp_port)
		:
			Open_socket(tcp_port),
			_io_buffer(env.ram(), env.rm(), io_buffer_size)
		{ }

		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() override { return Size(0, 0); }

		bool avail() override
		{
			bool ret = false;
			Libc::with_libc([&] () { ret = !read_buffer_empty(); });
			return ret;
		}

		Genode::size_t _read(Genode::size_t dst_len)
		{
			Genode::size_t num_bytes = 0;
			Libc::with_libc([&] () {
				num_bytes = read_buffer(_io_buffer.local_addr<char>(),
				                        Genode::min(_io_buffer.size(), dst_len));

				/*
				 * If read buffer was in use, look if more data is buffered in
				 * the TCP/IP stack.
				 */
				if (num_bytes)
					open_socket_pool()->update_sockets_to_watch();
			});
			return num_bytes;
		}

		Genode::size_t _write(Genode::size_t num_bytes)
		{
			ssize_t written_bytes = 0;

			Libc::with_libc([&] () {
				/* sanitize argument */
				num_bytes = Genode::min(num_bytes, _io_buffer.size());

				/* write data to socket, assuming that it won't block */
				written_bytes = ::write(sd(),
				                        _io_buffer.local_addr<char>(),
				                        num_bytes);

				if (written_bytes < 0) {
					Genode::error("write error, dropping data");
					written_bytes = 0;
				}
			});

			return written_bytes;
		}

		Genode::Dataspace_capability _dataspace()
		{
			return _io_buffer.cap();
		}

		void read_avail_sigh(Genode::Signal_context_capability sigh) override
		{
			Open_socket::read_avail_sigh(sigh);
		}

		void connected_sigh(Genode::Signal_context_capability sigh) override
		{
			Open_socket::connected_sigh(sigh);
		}

		void size_changed_sigh(Genode::Signal_context_capability) override { }

		Genode::size_t read(void *buf, Genode::size_t) override { return 0; }
		Genode::size_t write(void const *buf, Genode::size_t) override { return 0; }
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env &_env;
		Genode::Xml_node  _config;

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			/*
			 * XXX read I/O buffer size from args
			 */
			Genode::size_t io_buffer_size = 4096;

			try {
				Session_label const label = label_from_args(args);
				Session_policy policy(label, _config);

				unsigned tcp_port = 0;
				policy.attribute("port").value(&tcp_port);
				Session_component *session = nullptr;
				Libc::with_libc([&] () {
					session = new (md_alloc())
						Session_component(_env, io_buffer_size, tcp_port);
				});
				return session;
			}
			catch (Xml_node::Nonexistent_attribute) {
				error("Missing \"port\" attribute in policy definition");
				throw Service_denied();
			}
			catch (Session_policy::No_policy_defined) {
				error("Invalid session request, no matching policy");
				throw Service_denied();
			}
		}

	public:

		/**
		 * Constructor
		 */
		Root_component(Genode::Env       &env,
		               Genode::Xml_node   config,
		               Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(),
			                                          &md_alloc),
			_env(env), _config(config)
		{ }
};


struct Main
{
	Genode::Env &_env;

	Genode::Attached_rom_dataspace  _config_rom { _env, "config" };
	Genode::Xml_node                _config     { _config_rom.xml() };

	Genode::Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	/* create root interface for service */
	Terminal::Root_component _root { _env, _config, _sliced_heap };

	Main(Genode::Env &env) : _env(env)
	{
		Genode::log("--- TCP terminal started ---");
		Libc::with_libc([&] () {
			/* start thread blocking in select */
			open_socket_pool(&_env);
		});

		/* announce service at our parent */
		_env.parent().announce(env.ep().manage(_root));
	}
};

void Libc::Component::construct(Libc::Env &env) { static Main main(env); }

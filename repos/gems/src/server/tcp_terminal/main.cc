/*
 * \brief  Service providing the 'Terminal_session' interface via TCP
 * \author Norman Feske
 * \date   2011-09-07
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/list.h>
#include <util/misc_math.h>
#include <base/log.h>
#include <base/rpc_server.h>
#include <base/heap.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>
#include <cap_session/connection.h>
#include <os/attached_ram_dataspace.h>
#include <os/session_policy.h>

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

	public:

		Open_socket_pool() : _count(0)
		{
			pipe(sync_pipe_fds);
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


Open_socket_pool *open_socket_pool()
{
	static Open_socket_pool inst;
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
	close(_sd);
	open_socket_pool()->remove(this);
}


namespace Terminal {

	class Session_component : public Genode::Rpc_object<Session, Session_component>,
	                          public Open_socket
	{
		private:

			Genode::Attached_ram_dataspace _io_buffer;

		public:

			Session_component(Genode::size_t io_buffer_size, int tcp_port)
			:
				Open_socket(tcp_port),
				_io_buffer(Genode::env()->ram_session(), io_buffer_size)
			{ }

			/********************************
			 ** Terminal session interface **
			 ********************************/

			Size size() { return Size(0, 0); }

			bool avail()
			{
				return !read_buffer_empty();
			}

			Genode::size_t _read(Genode::size_t dst_len)
			{
				Genode::size_t num_bytes =
					read_buffer(_io_buffer.local_addr<char>(),
					            Genode::min(_io_buffer.size(), dst_len));

				/*
				 * If read buffer was in use, look if more data is buffered in
				 * the TCP/IP stack.
				 */
				if (num_bytes)
					open_socket_pool()->update_sockets_to_watch();

				return num_bytes;
			}

			void _write(Genode::size_t num_bytes)
			{
				/* sanitize argument */
				num_bytes = Genode::min(num_bytes, _io_buffer.size());

				/* write data to socket, assuming that it won't block */
				if (::write(sd(), _io_buffer.local_addr<char>(), num_bytes) < 0)
					Genode::error("write error, dropping data");
			}

			Genode::Dataspace_capability _dataspace()
			{
				return _io_buffer.cap();
			}

			void read_avail_sigh(Genode::Signal_context_capability sigh)
			{
				Open_socket::read_avail_sigh(sigh);
			}

			void connected_sigh(Genode::Signal_context_capability sigh)
			{
				Open_socket::connected_sigh(sigh);
			}

			Genode::size_t read(void *buf, Genode::size_t) { return 0; }
			Genode::size_t write(void const *buf, Genode::size_t) { return 0; }
	};


	class Root_component : public Genode::Root_component<Session_component>
	{
		protected:

			Session_component *_create_session(const char *args)
			{
				using namespace Genode;

				/*
				 * XXX read I/O buffer size from args
				 */
				size_t io_buffer_size = 4096;

				try {
					Session_label const label = label_from_args(args);
					Session_policy policy(label);

					unsigned tcp_port = 0;
					policy.attribute("port").value(&tcp_port);
					return new (md_alloc())
					       Session_component(io_buffer_size, tcp_port);

				} catch (Xml_node::Nonexistent_attribute) {
					error("Missing \"port\" attribute in policy definition");
					throw Root::Unavailable();
				} catch (Session_policy::No_policy_defined) {
					error("Invalid session request, no matching policy");
					throw Root::Unavailable();
				}
			}

		public:

			/**
			 * Constructor
			 */
			Root_component(Genode::Rpc_entrypoint *ep,
			               Genode::Allocator      *md_alloc)
			:
				Genode::Root_component<Session_component>(ep, md_alloc)
			{ }
	};
}


int main()
{
	using namespace Genode;

	Genode::log("--- TCP terminal started ---");

	/* initialize entry point that serves the root interface */
	enum { STACK_SIZE = 4*4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "terminal_ep");

	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	/* create root interface for service */
	static Terminal::Root_component root(&ep, &sliced_heap);

	/* announce service at our parent */
	env()->parent()->announce(ep.manage(&root));

	for (;;)
		open_socket_pool()->watch_sockets_for_incoming_data();

	return 0;
}

/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \date   2018-09-25
 *
 * On the local side this component provides Terminal sessions to its
 * configured clients while it also provides SSH sessions on the remote side.
 * The relation between both sides is establish via the policy settings that
 * determine which Terminal session may be accessed by a SSH login and the
 * other way around.
 *
 * When the component gets started it will create a read-only login database.
 * A login consists of a username and either a password or public-key or both.
 * The username is the unique primary key and is used to identify the right
 * Terminal session when a login is attempted. In return, it is also used to
 * attach a Terminal session to an (existing) SSH session. The SSH protocol
 * processing is done via libssh running in its own event thread while the
 * EP handles the Terminal session. Locking is done at the appropriate places
 * to synchronize both threads.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/rpc_server.h>
#include <base/registry.h>
#include <libc/component.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <pthread.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>
#include <util/list.h>
#include <util/misc_math.h>

/* libc includes */
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>

/* libssh includes */
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>


namespace Util {

	using Filename = Genode::String<256>;

	template <size_t C>
	struct Buffer
	{
		Genode::Lock _lock { };
		char         _data[C] { };
		size_t       _head { 0 };
		size_t       _tail { 0 };

		size_t read_avail()   const { return _head > _tail ? _head - _tail : 0; }
		size_t write_avail()  const { return _head <= C ? C - _head : 0; }
		char const *content() const { return &_data[_tail]; }

		Genode::Lock &lock() { return _lock; }
		void append(char c) { _data[_head++] = c; }
		void consume(size_t n) { _tail += n; }
		void reset() { _head = _tail = 0; }
	};

	char const *get_time()
	{
		static char buffer[32];

		char const *p = "<invalid date>";
		Libc::with_libc([&] {
			struct timespec ts;
			if (clock_gettime(0, &ts)) { return; }

			struct tm *tm = localtime((time_t*)&ts.tv_sec);
			if (!tm) { return; }

			size_t const n = strftime(buffer, sizeof(buffer), "%F %H:%M:%S", tm);
			if (n > 0 && n < sizeof(buffer)) { p = buffer; }
		}); /* Libc::with_libc */

		return p;
	}

} /* namespace Util */


/*
 * Add the libssh callback forward declarations here so that we can use them
 * from within the classes and thereby document the ones currently implemented.
 */

static int channel_data_cb(ssh_session, ssh_channel, void *, uint32_t, int, void *);
static int channel_env_request_cb(ssh_session, ssh_channel, char const *, char const *, void *);
static int channel_pty_request_cb(ssh_session, ssh_channel, char const *, int, int, int, int, void *);
static int channel_pty_window_change_cb(ssh_session, ssh_channel, int, int, int, int, void *);
static int channel_shell_request_cb(ssh_session, ssh_channel, void *);

static void bind_incoming_connection(ssh_bind, void *);
static int session_service_request_cb(ssh_session, char const *, void *);
static int session_auth_password_cb(ssh_session, char const *, char const *, void *);
static int session_auth_pubkey_cb(ssh_session, char const *, struct ssh_key_struct *, char, void *);
static ssh_channel session_channel_open_request_cb(ssh_session, void *);


namespace Ssh {

	using namespace Genode;

	using User     = String<32>;
	using Password = String<64>;
	using Hash     = String<65>;

	struct Login;
	struct Login_registry;

	struct Terminal;
	struct Server;

}; /* namespace Ssh */


struct Ssh::Login : Genode::Registry<Ssh::Login>::Element
{
	Ssh::User     user         { };
	Ssh::Password password     { };
	Ssh::Hash     pub_key_hash { };
	ssh_key       pub_key      { nullptr };

	bool multi_login      { false };
	bool request_terminal { false };

	/**
	 * Constructor
	 */
	Login(Genode::Registry<Login> &reg,
	      Ssh::User      const &user,
	      Ssh::Password  const &pw,
	      Util::Filename const &pk_file,
	      bool           const multi_login,
	      bool           const request_terminal)
	:
		Element(reg, *this),
		user(user), password(pw), multi_login(multi_login),
		request_terminal(request_terminal)
	{
		Libc::with_libc([&] {

			if (   pk_file.valid()
				&& ssh_pki_import_pubkey_file(pk_file.string(), &pub_key)) {
				Genode::error("could not import public key for user '",
				              user, "'");
			}

			if (pub_key) {
				unsigned char *h    = nullptr;
				size_t         hlen = 0;
				/* pray and assume both calls never fail */
				ssh_get_publickey_hash(pub_key, SSH_PUBLICKEY_HASH_SHA256,
				                       &h, &hlen);
				char const *p = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256,
				                                         h, hlen);
				if (p) { pub_key_hash = Ssh::Hash(p); }

				ssh_clean_pubkey_hash(&h);
				/* abuse function to free fingerprint */
				ssh_clean_pubkey_hash((unsigned char**)&p);
			}
		}); /* Libc::with_libc */
	}

	~Login() { ssh_key_free(pub_key); }

	bool auth_password()  const { return password.valid(); }
	bool auth_publickey() const { return pub_key != nullptr; }

	void print(Genode::Output &out) const
	{
		Genode::print(out, "user ", user, ": ");
		if (auth_password())  { Genode::print(out, "password "); }
		if (auth_publickey()) { Genode::print(out, "public-key"); }
	}
};


struct Ssh::Login_registry : Genode::Registry<Ssh::Login>
{
	Genode::Allocator &_alloc;

	Genode::Lock _lock { };

	/**
	 * Import one login from node
	 */
	bool _import_single(Genode::Xml_node const &node)
	{
		User user          = node.attribute_value("user",        User());
		Password pw        = node.attribute_value("password",    Password());
		Util::Filename pub = node.attribute_value("pub_key",     Util::Filename());
		bool multi_login   = node.attribute_value("multi_login", false);
		bool req_term      = node.attribute_value("request_terminal", false);

		if (!user.valid() || (!pw.valid() && !pub.valid())) {
			Genode::warning("ignoring invalid policy");
			return false;
		}

		if (lookup(user.string())) {
			Genode::warning("ignoring already imported login ", user.string());
			return false;
		}

		try {
			new (&_alloc) Login(*this, user, pw, pub, multi_login, req_term);
			return true;
		} catch (...) { return false; }
	}

	void _remove_all()
	{
		for_each([&] (Login &login) {
			Genode::destroy(&_alloc, &login);
		});
	}

	/**
	 * Constructor
	 *
	 * \param  alloc   allocator for registry elements
	 */
	Login_registry(Genode::Allocator &alloc) : _alloc(alloc) { }

	/**
	 * Return registry lock
	 */
	Genode::Lock &lock() { return _lock; }

	/**
	 * Import all login information from config
	 */
	void import(Genode::Xml_node const &node)
	{
		_remove_all();

		try {
			node.for_each_sub_node("policy",
			[&] (Genode::Xml_node const &node) {
				_import_single(node);
			});
		} catch (...) { }
	}

	/**
	 * Look up login information by user name
	 */
	Ssh::Login const *lookup(char const *user) const
	{
		Login const *p = nullptr;
		auto lookup_user = [&] (Login const &login) {
			if (login.user == user) { p = &login; }
		};
		for_each(lookup_user);
		return p;
	}
};


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

		~Terminal() { }

		Ssh::User const &user() const { return _user; }

		unsigned attached_channels() const { return _attached_channels; }

		void attach_channel() { ++_attached_channels; }

		void detach_channel() { --_attached_channels; }

		void reset_pending() { _pending_channels = 0; }

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
		void connected_sigh(Signal_context_capability sigh) {
			_connected_sigh = sigh; }

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
			while (_write_buf.write_avail() > 0 && i < src_len) {
				char c = src[i];

				if (c == '\n') { _write_buf.append('\r'); }

				_write_buf.append(c);
				num_bytes++;

				i++;
			}

			/* wake the event loop up */
			char c = 1;
			::write(write_avail_fd, &c, sizeof(c));

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


class Ssh::Server
{
	public:

		struct Init_failed    : Genode::Exception { };
		struct Invalid_config : Genode::Exception { };

		struct Session : Genode::Registry<Session>::Element
		{
			User     _user { };
			uint32_t _id   { 0 };

			int  bad_auth_attempts { 0 };
			bool auth_sucessful    { false };

			ssh_session session { nullptr };
			ssh_channel channel { nullptr };
			ssh_channel_callbacks channel_cb { nullptr };

			Ssh::Terminal *terminal { nullptr };

			Genode::Lock _access_lock { };
			Genode::Lock &lock_terminal()
			{
				return _access_lock;
			}

			bool spawn_terminal { false };

			Session(Genode::Registry<Session> &reg,
			        ssh_session s,
			        ssh_channel_callbacks ccb,
			        uint32_t id)
			: Element(reg, *this), _id(id), session(s), channel_cb(ccb) { }

			void adopt(User const &user) { _user = user; }

			User const &user() const { return _user; }
			uint32_t      id() const { return _id; }

			void add_channel(ssh_channel c)
			{
				ssh_set_channel_callbacks(c, channel_cb);
				channel = c;
			}
		};

	private:

		Genode::Env  &_env;
		Genode::Heap  _heap;

		bool _verbose         { false };
		bool _allow_password  { false };
		bool _allow_publickey { false };
		bool _log_logins      { false };

		int      _max_auth_attempts { 3 };
		unsigned _port              { 0u };
		unsigned _log_level         { 0u };

		ssh_bind                  _ssh_bind;
		ssh_event _event_loop;
		ssh_key pub_key;

		/*
		 * Since we always pass ourself as userdata pointer, we may
		 * safely use the same callback for all sessions and channels.
		 */
		ssh_channel_callbacks_struct _channel_cb { };
		ssh_server_callbacks_struct  _session_cb { };
		ssh_bind_callbacks_struct    _bind_cb    { };

		void _initialize_channel_callbacks()
		{
			Genode::memset(&_channel_cb, 0, sizeof(_channel_cb));

			_channel_cb.userdata                           = this;
			_channel_cb.channel_data_function              = channel_data_cb;
			_channel_cb.channel_env_request_function       = channel_env_request_cb;
			_channel_cb.channel_pty_request_function       = channel_pty_request_cb;
			_channel_cb.channel_pty_window_change_function = channel_pty_window_change_cb;
			_channel_cb.channel_shell_request_function     = channel_shell_request_cb;

			ssh_callbacks_init(&_channel_cb);
		}

		void _initialize_session_callbacks()
		{
			Genode::memset(&_session_cb, 0, sizeof(_session_cb));

			_session_cb.userdata                              = this;
			_session_cb.auth_password_function                = session_auth_password_cb;
			_session_cb.auth_pubkey_function                  = session_auth_pubkey_cb;
			_session_cb.service_request_function              = session_service_request_cb;
			_session_cb.channel_open_request_session_function = session_channel_open_request_cb;

			ssh_callbacks_init(&_session_cb);
		}

		void _initialize_bind_callbacks()
		{
			Genode::memset(&_bind_cb,  0, sizeof(_bind_cb));
			_bind_cb.incoming_connection = bind_incoming_connection;
			ssh_callbacks_init(&_bind_cb);
			ssh_bind_set_callbacks(_ssh_bind, &_bind_cb, this);
		}

		using Session_registry = Genode::Registry<Session>;
		Session_registry _sessions { };
		uint32_t         _session_id { 0 };

		void _cleanup_session(Session &s)
		{
			if (s.auth_sucessful) { _log_logout(s); }

			ssh_channel_free(s.channel);
			s.channel = nullptr;

			ssh_event_remove_session(_event_loop, s.session);
			ssh_disconnect(s.session);
			ssh_free(s.session);
			s.session = nullptr;

			if (s.terminal) {
				s.terminal->detach_channel();
			}

			Genode::destroy(&_heap, &s);
		}

		void _cleanup_sessions()
		{
			auto cleanup = [&] (Session &s) {
				if (!ssh_is_connected(s.session)) {
					_cleanup_session(s);
				}
			};
			_sessions.for_each(cleanup);
		}

		struct Terminal_session : Genode::Registry<Terminal_session>::Element
		{
			Ssh::Terminal &conn;

			ssh_event _event_loop;

			int _fds[2] { -1, -1 };

			Terminal_session(Genode::Registry<Terminal_session> &reg,
			                 Ssh::Terminal &conn,
			                 ssh_event event_loop)
			: Element(reg, *this), conn(conn), _event_loop(event_loop)
			{
				if (pipe(_fds) ||
				    ssh_event_add_fd(_event_loop, _fds[0], POLLIN,
				                     write_avail_cb, this) != SSH_OK) {
					throw -1;
				}

				conn.write_avail_fd = _fds[1];
			}

			~Terminal_session()
			{
				ssh_event_remove_fd(_event_loop, _fds[0]);
				close(_fds[0]);
				close(_fds[1]);
			}
		};

		struct Terminal_registry : Genode::Registry<Terminal_session>
		{
			Genode::Lock _lock { };
			Genode::Lock &lock() { return _lock; }
		};
		Terminal_registry _terminals { };

		Login_registry &_logins;

		Util::Filename rsa_key     { };
		Util::Filename ecdsa_key   { };
		Util::Filename ed25519_key { };

		bool _config_once { false };

		void _parse_config(Genode::Xml_node const &config)
		{
			using namespace Genode;

			_verbose = config.attribute_value("verbose", false);

			_log_level       = config.attribute_value("debug", 0u);
			_log_logins      = config.attribute_value("log_logins",      true);

			Genode::Lock::Guard g(_logins.lock());
			auto print = [&] (Login const &login) {
				Genode::log("Login configured: ", login);
			};
			_logins.for_each(print);

			if (_config_once) { return; }

			_config_once = true;

			_port = config.attribute_value("port", 0u);
			if (!_port) {
				error("port invalid");
				throw Invalid_config();
			}

			_allow_password  = config.attribute_value("allow_password",  false);
			_allow_publickey = config.attribute_value("allow_publickey", false);
			if (!_allow_password && !_allow_publickey) {
				error("authentication methods missing");
				throw Invalid_config();
			}

			rsa_key     = config.attribute_value("rsa_key",     Util::Filename());
			ecdsa_key   = config.attribute_value("ecdsa_key",   Util::Filename());
			ed25519_key = config.attribute_value("ed25519_key", Util::Filename());

			Genode::log("Allowed auth methods: ",
			            _allow_password  ? "password "  : "",
			            _allow_publickey ? "public-key" : "");
		}

		void _load_hostkey(Util::Filename const &file)
		{
			if (file.valid() &&
			    ssh_bind_options_set(_ssh_bind, SSH_BIND_OPTIONS_HOSTKEY,
			                         file.string()) < 0) {
				Genode::error("could not load hostkey '", file, "'");
				throw -1;
			}
		}

		/*
		 * Event execution
		 */

		pthread_t _event_thread;

		static void *server_loop(void *arg)
		{
			Ssh::Server *server = reinterpret_cast<Ssh::Server *>(arg);
			Libc::with_libc([&] {
				server->loop();
			});
			return nullptr;
		}

		static int write_avail_cb(socket_t fd, int revents, void *userdata)
		{
			int n = 0;
			Libc::with_libc([&] {
				char c;
				n = ::read(fd, &c, sizeof(char));
			});
			return n;
		}

		bool _allow_multi_login(ssh_session s, Login const &login)
		{
			if (login.multi_login) { return true; }

			bool found = false;
			auto lookup = [&] (Session const &s) {
				if (s.user() == login.user) { found = true; }
			};
			_sessions.for_each(lookup);
			return !found;
		}

		/********************
		 ** Login messages **
		 ********************/

		void _log_failed(char const *user, Session const &s, bool pubkey)
		{
			if (!_log_logins) { return; }

			char const *date = Util::get_time();
			Genode::log(date, " failed user ", user, " (", s.id(), ") ",
			            "with ", pubkey ? "public-key" : "password");
		}

		void _log_logout(Session const &s)
		{
			if (!_log_logins) { return; }

			char const *date = Util::get_time();
			Genode::log(date, " logout user ", s.user(), " (", s.id(), ")");
		}

		void _log_login(User const &user, Session const &s, bool pubkey)
		{
			if (!_log_logins) { return; }

			char const *date = Util::get_time();
			Genode::log(date, " login user ", user, " (", s.id(), ") ",
			            "with ", pubkey ? "public-key" : "password");
		}

	public:

		Server(Genode::Env &env,
		       Genode::Xml_node const &config,
		       Ssh::Login_registry    &logins)
		: _env(env), _heap(env.ram(), env.rm()), _logins(logins)
		{
			Libc::with_libc([&] {
				_parse_config(config);

				if (ssh_init() < 0) { throw Init_failed(); }

				_ssh_bind = ssh_bind_new();
				if (!_ssh_bind) { throw Init_failed(); }

				ssh_bind_options_set(_ssh_bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &_log_level);
				ssh_bind_options_set(_ssh_bind, SSH_BIND_OPTIONS_BINDPORT, &_port);

				_initialize_bind_callbacks();
				_initialize_session_callbacks();
				_initialize_channel_callbacks();

				/*
				 * Always try to load all types of host key and error-out if
				 * the file is set but could not be loaded.
				 */
				try {
					_load_hostkey(rsa_key);
					_load_hostkey(ecdsa_key);
					_load_hostkey(ed25519_key);
				} catch (...) { throw Init_failed(); }

				_event_loop = ssh_event_new();

				if (ssh_bind_listen(_ssh_bind) < 0) {
					Genode::error("could not listen on port ", _port, ": ",
					              ssh_get_error(_ssh_bind));
					throw Init_failed();
				}

				/* add AFTER(!) ssh_bind_listen call */
				if (ssh_event_add_bind(_event_loop, _ssh_bind) < 0) {
					Genode::error("unable to add server to event loop: ",
					            ssh_get_error(_ssh_bind));
					throw Init_failed();
				}

				if (pthread_create(&_event_thread, nullptr, server_loop, this)) {
					Genode::error("could not create event thread");
					throw Init_failed();
				}

				Genode::log("Listen on port: ", _port);
			}); /* Libc::with_libc */
		}

		void loop()
		{
			while (true) {


				int const events = ssh_event_dopoll(_event_loop, -1);
				if (events < 0) { _cleanup_sessions(); }

				{
					Genode::Lock::Guard g(_terminals.lock());

					/* first remove all stale sessions */
					auto cleanup = [&] (Session &s) {
						if (ssh_is_connected(s.session)) { return ; }
						_cleanup_session(s);
					};
					_sessions.for_each(cleanup);

					/* second reset all active terminals */
					auto reset_pending = [&] (Terminal_session &t) {
						if (!t.conn.attached_channels()) { return; }
						t.conn.reset_pending();
					};
					_terminals.for_each(reset_pending);

					/*
					 * third send data on all sessions being attached
					 * to a terminal.
					 */
					auto send = [&] (Session &s) {
						if (!s.terminal) { return; }

						try { s.terminal->send(s.channel); }
						catch (...) { _cleanup_session(s); }
					};
					_sessions.for_each(send);
				}
			}
		}

		/***************************************************************
		 ** Methods below are only used by Terminal session front end **
		 ***************************************************************/

		/**
		 * Attach Terminal session
		 */
		void attach_terminal(Ssh::Terminal &conn)
		{
			Genode::Lock::Guard g(_terminals.lock());

			try {
				new (&_heap) Terminal_session(_terminals,
				                              conn, _event_loop);
			} catch (...) {
				Genode::error("could not attach Terminal for user ", conn.user());
				throw -1;
			}

			Genode::log("Attach Terminal for user ", conn.user());

			/* there might be sessions already waiting on the terminal */
			bool attached = false;
			auto lookup = [&] (Session &s) {
				if (s.user() == conn.user() && !s.terminal) {
					s.terminal = &conn;
					s.terminal->attach_channel();

					attached = true;
				}
			};
			_sessions.for_each(lookup);

			if (attached) { conn.notify_connected(); }
		}

		/**
		 * Detach Terminal session
		 */
		void detach_terminal(Ssh::Terminal &conn)
		{
			Genode::Lock::Guard g(_terminals.lock());

			Terminal_session *p = nullptr;
			auto lookup = [&] (Terminal_session &t) {
				if (&t.conn == &conn) { p = &t; }
			};
			_terminals.for_each(lookup);

			if (!p) {
				Genode::error("could not detach Terminal for user ", conn.user());
				return;
			}
			auto invalidate_terminal = [&] (Session &sess) {
				if (sess.terminal != &conn) { return; }

				sess.terminal = nullptr;
			};
			_sessions.for_each(invalidate_terminal);

			Genode::log("Detach Terminal ", &conn, " for user ", conn.user());
			Genode::destroy(&_heap, p);
		}

		/**
		 * Update config
		 */
		void update_config(Genode::Xml_node const &config)
		{
			Genode::Lock::Guard g(_terminals.lock());

			_parse_config(config);
			ssh_bind_options_set(_ssh_bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &_log_level);
		}

		/*******************************************************
		 ** Methods below are only used by callback back ends **
		 *******************************************************/

		/**
		 * Look up Terminal for session
		 */
		Ssh::Terminal *lookup_terminal(Session &s)
		{
			Ssh::Terminal *p = nullptr;
			auto lookup = [&] (Terminal_session &t) {
				if (t.conn.user() == s.user()) { p = &t.conn; }
			};
			_terminals.for_each(lookup);
			return p;
		}

		/**
		 * Look up Session for SSH session
		 */
		Session *lookup_session(ssh_session s)
		{
			Session *p = nullptr;
			auto lookup = [&] (Session &sess) {
				if (sess.session == s) { p = &sess; }
			};
			_sessions.for_each(lookup);
			return p;
		}

		/**
		 * Request Terminal
		 */
		bool request_terminal(Session &session)
		{
			Genode::Lock::Guard g(_logins.lock());
			Login const *l = _logins.lookup(session.user().string());
			if (!l || !l->request_terminal) { return false; }

			try {
				Genode::String<64> report_label("request_terminal ", session.user());
				Reporter reporter(_env, "request_terminal", report_label.string());
				reporter.enabled(true);

				Genode::Reporter::Xml_generator xml(reporter, [&] {
					xml.attribute("user", session.user());
				});
			} catch (...) {
				Genode::warning("could not enable login reporting");
				return false;
			}

			if (_log_logins) {
				char const *date = Util::get_time();
				Genode::log(date, " request Terminal for user ", session.user(),
				            " (", session.session, ")");
			}

			return true;
		}

		/**
		 * Handle new incoming connections
		 */
		void incoming_connection(ssh_session s)
		{
			/*
			 * In case we get bombarded by incoming connections, deny
			 * all attempts when this arbritray level is reached.
			 */
			enum { MEM_RESERVE = 128u * 1024, };
			if (_env.pd().avail_ram().value < (size_t)MEM_RESERVE) { throw -1; }

			new (&_heap) Session(_sessions, s, &_channel_cb, ++_session_id);

			ssh_set_server_callbacks(s, &_session_cb);

			int auth_methods = SSH_AUTH_METHOD_UNKNOWN;
			auth_methods += _allow_password  ? SSH_AUTH_METHOD_PASSWORD  : 0;
			auth_methods += _allow_publickey ? SSH_AUTH_METHOD_PUBLICKEY : 0;
			ssh_set_auth_methods(s, auth_methods);

			/*
			 * Normally we would check the result of the key exchange
			 * function but for better or worse using callbacks leads to
			 * a false negative. So ignore any result and move on in hope
			 * that the callsbacks will handle the situation.
			 *
			 * FIXME investigate why it somtimes fails in the first place.
			 */
			ssh_handle_key_exchange(s);
			ssh_event_add_session(_event_loop, s);
		}

		/**
		 * Handle password authentication
		 */
		bool auth_password(ssh_session s, char const *u, char const *pass)
		{
			Session *p = lookup_session(s);
			if (!p || p->session != s) {
				Genode::warning("session not found");
				return false;
			}
			Session &session = *p;

			/*
			 * Even if there is no valid login for the user, let
			 * the client try anyway and check multi login afterwards.
			 */
			Genode::Lock::Guard g(_logins.lock());
			Login const *l = _logins.lookup(u);
			if (l && l->user == u && l->password == pass) {
				if (_allow_multi_login(s, *l)) {
					session.bad_auth_attempts = 0;
					session.auth_sucessful = true;
					session.adopt(l->user);
					_log_login(l->user, session, false);
					return true;
				} else {
					ssh_disconnect(session.session);
					_log_failed(u, session, false);
					return false;
				}
			}

			_log_failed(u, *p, false);

			int &i = session.bad_auth_attempts;
			if (++i >= _max_auth_attempts) {
				if (_log_logins) {
					char const *date = Util::get_time();
					Genode::log(date, " disconnect user ", u, " (", session.id(),
					            ") after ", i, " failed authentication attempts"
					            " with password");
				}
				ssh_disconnect(session.session);
			}
			return false;
		}

		/**
		 * Handle public-key authentication
		 */
		bool auth_pubkey(ssh_session s, char const *u,
		                 struct ssh_key_struct *pubkey,
                         char signature_state)
		{
			Session *p = lookup_session(s);
			if (!p || p->session != s) {
				Genode::warning("session not found");
				return false;
			}
			Session &session = *p;

			if (signature_state == SSH_PUBLICKEY_STATE_NONE) {
				return SSH_AUTH_PARTIAL;
			}

			if (signature_state == SSH_PUBLICKEY_STATE_VALID) {
				Genode::Lock::Guard g(_logins.lock());
				Login const *l = _logins.lookup(u);
				if (l && !ssh_key_cmp(pubkey, l->pub_key,
				                      SSH_KEY_CMP_PUBLIC)) {
					if (_allow_multi_login(s, *l)) {
						session.auth_sucessful = true;
						session.adopt(l->user);
						_log_login(l->user, session, true);
						return SSH_AUTH_SUCCESS;
					}
				}
			}

			_log_failed(u, session, true);
			return SSH_AUTH_DENIED;
		}
};


/***********************
 ** Channel callbacks **
 ***********************/

/**
 * Handle SSH channel data request
 *
 * For now we ignore this request because there is no way to change the
 * $ENV of the Terminal::Session client currently.
 */
static int channel_data_cb(ssh_session session, ssh_channel channel,
                           void *data, uint32_t len, int is_stderr,
                           void *userdata)
{
	if (len == 0) { return 0; }

	using namespace Genode;
	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Server::Session *p = server.lookup_session(session);
	if (!p) {
		error("session not found");
		return SSH_ERROR;
	}

	if (p->channel != channel) {
		error("wrong channel");
		return SSH_ERROR;
	}

	if (!p->terminal) { return SSH_ERROR; }

	Ssh::Terminal &conn = *p->terminal;
	Lock::Guard g(conn.read_buf.lock());

	char const *src = reinterpret_cast<char const*>(data);
	size_t num_bytes = 0;
	size_t i = 0;

	while (conn.read_buf.write_avail() > 0 && i < len) {
		char c = src[i];

		/* replace ^? with ^H and let's hope we do not break anything */
		enum { DEL = 0x7f, BS = 0x08, };
		if (c == DEL) { conn.read_buf.append(BS); }
		else          { conn.read_buf.append(c); }
		num_bytes++;
		i++;
	}

	conn.notify_read_avail();
	return num_bytes;
}


/**
 * Handle SSH channel shell request
 *
 * For now we ignore this request because there is no way to change the
 * $ENV of the Terminal::Session client currently.
 */
static int channel_env_request_cb(ssh_session session, ssh_channel channel,
                                  char const *env_name, char const *env_value,
                                  void *userdata)
{
	return SSH_OK;
}


/**
 * Handle SSH channel PTY resize request
 */
static int channel_pty_request_cb(ssh_session session, ssh_channel channel,
                                  char const *term,
                                  int cols, int rows, int py, int px,
                                  void *userdata)
{
	using namespace Genode;
	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Server::Session *p = server.lookup_session(session);
	if (!p || p->channel != channel) { return SSH_ERROR; }

	/*
	 * Look up terminal and in case there is none, check
	 * if we have to wait for another subsystem to come up.
	 * In this case we return successfully to the client
	 * and hope for the best.
	 */
	if (!p->terminal) {
		p->terminal = server.lookup_terminal(*p);
		if (!p->terminal) {
			return server.request_terminal(*p) ? SSH_OK
			                                   : SSH_ERROR;
		}
	}

	p->terminal->attach_channel();

	Ssh::Terminal &conn = *p->terminal;
	conn.size(Terminal::Session::Size(cols, rows));
	conn.notify_size_changed();

	/* session handling already takes care of having a terminal attached */
	conn.notify_connected();
	return SSH_OK;
}


/**
 * Handle SSH channel PTY resize request
 */
static int channel_pty_window_change_cb(ssh_session session, ssh_channel channel,
                                        int width, int height, int pxwidth, int pwheight,
                                        void *userdata)
{
	(void)pxwidth;
	(void)pwheight;

	using namespace Genode;
	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Server::Session *p = server.lookup_session(session);
	if (!p || p->channel != channel || !p->terminal) { return SSH_ERROR; }

	Ssh::Terminal &conn = *p->terminal;
	conn.size(Terminal::Session::Size(width, height));
	conn.notify_size_changed();
	return SSH_OK;
}


/**
 * Handle SSH channel shell request
 *
 * For now we ignore this request as the shell is implicitly provided when
 * the PTY request is handled.
 */
static int channel_shell_request_cb(ssh_session session, ssh_channel channel,
                                    void *userdata)
{
	return SSH_OK;
}


/***********************
 ** Session callbacks **
 ***********************/

/**
 * Handle SSH session service requests
 */
static int session_service_request_cb(ssh_session session,
                                      char const *service, void *userdata)
{
	(void)service;
	(void)userdata;

	return Genode::strcmp(service, "ssh-userauth") == 0 ? 0 : -1;
}


/**
 * Handle SSH session password authentication requests
 */
static int session_auth_password_cb(ssh_session session,
                                    char const *user, char const *password,
                                    void *userdata)
{
	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	return server.auth_password(session, user, password) ? SSH_AUTH_SUCCESS
	                                                     : SSH_AUTH_DENIED;
}


/**
 * Handle SSH session public-key authentication requests
 */
static int session_auth_pubkey_cb(ssh_session session, char const *user,
                                  struct ssh_key_struct *pubkey,
                                  char state, void *userdata)
{
	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	return server.auth_pubkey(session, user, pubkey, state) ? SSH_AUTH_SUCCESS
	                                                        : SSH_AUTH_DENIED;

}


/**
 * Handle SSH session open channel requests
 */
static ssh_channel session_channel_open_request_cb(ssh_session session,
                                                   void *userdata)
{
	using namespace Genode;
	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Server::Session *p = server.lookup_session(session);
	if (!p) {
		error("could not look up session");
		return nullptr;
	}

	/* for now only one channel */
	if (p->channel) {
		log("Only one channel per session supported");
		return nullptr;
	}

	ssh_channel channel = ssh_channel_new(p->session);

	if (!channel) {
		error("could not create new channel: '", ssh_get_error(p->session));
		return nullptr;
	}

	p->add_channel(channel);
	return channel;
}


/**
 * Handle new incoming SSH session requests
 */
static void bind_incoming_connection(ssh_bind sshbind, void *userdata)
{
	using namespace Genode;

	ssh_session session = ssh_new();
	if (!session || ssh_bind_accept(sshbind, session)) {
		error("could not accept session: '", ssh_get_error(session), "'");
		ssh_free(session);
		return;
	}

	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	try { server.incoming_connection(session); }
	catch (...) {
		ssh_disconnect(session);
		ssh_free(session);
	}
}


/*
 * Terminal session front end
 */

namespace Terminal {
	class Session_component;
	class Root_component;
};

class Terminal::Session_component : public Genode::Rpc_object<Session, Session_component>,
                                    public Ssh::Terminal
{
	private:

		Genode::Attached_ram_dataspace _io_buffer;

	public:

		Session_component(Genode::Env &env,
		                  Genode::size_t io_buffer_size,
		                  Ssh::User const &user)
		:
			Ssh::Terminal(user),
			_io_buffer(env.ram(), env.rm(), io_buffer_size)
		{ }

		/********************************
		 ** Terminal session interface **
		 ********************************/

		Genode::size_t read(void *buf, Genode::size_t)        override { return 0; }
		Genode::size_t write(void const *buf, Genode::size_t) override { return 0; }

		Size size()  override { return Ssh::Terminal::size(); }
		bool avail() override { return !Ssh::Terminal::read_buffer_empty(); }

		void read_avail_sigh(Genode::Signal_context_capability sigh) override {
			Ssh::Terminal::read_avail_sigh(sigh); }

		void connected_sigh(Genode::Signal_context_capability sigh) override {
			Ssh::Terminal::connected_sigh(sigh); }

		void size_changed_sigh(Genode::Signal_context_capability sigh) override {
			Ssh::Terminal::size_changed_sigh(sigh); }

		Genode::Dataspace_capability _dataspace() { return _io_buffer.cap(); }

		Genode::size_t _read(Genode::size_t num)
		{
			Genode::size_t num_bytes = 0;
			Libc::with_libc([&] () {
				char *buf = _io_buffer.local_addr<char>();
				num = Genode::min(_io_buffer.size(), num);
				num_bytes = Ssh::Terminal::read(buf, num);
			});
			return num_bytes;
		}

		Genode::size_t _write(Genode::size_t num)
		{
			ssize_t written = 0;
			Libc::with_libc([&] () {
				char *buf = _io_buffer.local_addr<char>();
				num = Genode::min(num, _io_buffer.size());
				written = Ssh::Terminal::write(buf, num);

				if (written < 0) {
					Genode::error("write error, dropping data");
					written = 0;
				}
			});
			return written;
		}
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env &_env;

		Genode::Attached_rom_dataspace  _config_rom { _env, "config" };
		Genode::Xml_node                _config     { _config_rom.xml() };

		Genode::Heap        _logins_alloc { _env.ram(), _env.rm() };
		Ssh::Login_registry _logins       { _logins_alloc };

		Ssh::Server _server { _env, _config, _logins };

		Genode::Signal_handler<Terminal::Root_component> _config_sigh {
			_env.ep(), *this, &Terminal::Root_component::_handle_config_update };

		void _handle_config_update()
		{
			_config_rom.update();
			if (!_config_rom.valid()) { return; }

			{
				Genode::Lock::Guard g(_logins.lock());
				_logins.import(_config_rom.xml());
			}

			_server.update_config(_config_rom.xml());
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			try {
				Session_label const label = label_from_args(args);
				Session_policy policy(label, _config);

				Ssh::User const user = policy.attribute_value("user", Ssh::User());
				if (!user.valid()) { throw -1; }

				Ssh::Login const *login = _logins.lookup(user.string());
				if (!login) { throw -1; }

				Session_component *s = nullptr;
				Libc::with_libc([&] () {
					s = new (md_alloc()) Session_component(_env, 4096, login->user);
				});

				try {
					_server.attach_terminal(*s);
					return s;
				} catch (...) {
					Genode::destroy(md_alloc(), s);
					throw;
				}
			} catch (...) { throw Service_denied(); }
		}

		void _destroy_session(Session_component *s)
		{
			_server.detach_terminal(*s);
			Genode::destroy(md_alloc(), s);
		}

	public:

		Root_component(Genode::Env       &env,
		               Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(),
			                                          &md_alloc),
			_env(env)
		{
			_config_rom.sigh(_config_sigh);
			_handle_config_update();
		}
};


struct Main
{
	Genode::Env &_env;

	Genode::Sliced_heap      _sliced_heap { _env.ram(), _env.rm() };
	Terminal::Root_component _root        { _env, _sliced_heap };

	Main(Genode::Env &env) : _env(env)
	{
		Genode::log("--- SSH terminal started ---");
		_env.parent().announce(env.ep().manage(_root));
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main main(env); }

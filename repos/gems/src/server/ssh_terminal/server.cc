/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \author Pirmin Duss
 * \author Sid Hussmann
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* local includes */
#include "server.h"

/*
 * Add the libssh callback forward declarations here so that we can use them
 * from within the classes and thereby document the ones currently implemented.
 */

extern int channel_data_cb(ssh_session, ssh_channel, void *, uint32_t, int, void *);
extern int channel_env_request_cb(ssh_session, ssh_channel, char const *, char const *, void *);
extern int channel_pty_request_cb(ssh_session, ssh_channel, char const *, int, int, int, int, void *);
extern int channel_pty_window_change_cb(ssh_session, ssh_channel, int, int, int, int, void *);
extern int channel_shell_request_cb(ssh_session, ssh_channel, void *);
extern int channel_exec_request_cb(ssh_session, ssh_channel, char const *, void *);

extern void bind_incoming_connection(ssh_bind, void *);
extern int session_service_request_cb(ssh_session, char const *, void *);
extern int session_auth_password_cb(ssh_session, char const *, char const *, void *);
extern int session_auth_pubkey_cb(ssh_session, char const *, struct ssh_key_struct *, char, void *);
extern ssh_channel session_channel_open_request_cb(ssh_session, void *);

/**
 * forward declaration of the write available callback.
 */
static int write_avail_cb(socket_t fd, int revents, void *userdata);


Ssh::Terminal_session::Terminal_session(Genode::Registry<Terminal_session> &reg,
                                        Ssh::Terminal &conn,
                                        ssh_event event_loop)
:
	Element(reg, *this), conn(conn), _event_loop(event_loop)
{
	if (pipe(_fds) ||
		ssh_event_add_fd(_event_loop,
		                 _fds[0],
		                 POLLIN,
		                 write_avail_cb,
		                 this) != SSH_OK ) {
		Genode::error("Failed to create wakeup pipe");
		throw -1;
	}
	conn.write_avail_fd = _fds[1];
}


Ssh::Server::Server(Genode::Env &env,
                    Genode::Xml_node const &config,
                    Ssh::Login_registry    &logins)
:
	_env(env), _heap(env.ram(), env.rm()), _logins(logins)
{
	Libc::with_libc([&] {

		_parse_config(config);

		if (ssh_init() < 0) {
			Genode::error("ssh_init failed.");
			throw Init_failed();
		}

		_ssh_bind = ssh_bind_new();
		if (!_ssh_bind) {
			Genode::error("ssh_bind failed.");
			throw Init_failed();
		}

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
			_load_hostkey(_rsa_key);
			_load_hostkey(_ecdsa_key);
			_load_hostkey(_ed25519_key);
		} catch (...) {
			Genode::error("loading keys failed.");
			throw Init_failed();
		}

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

		if (pthread_create(&_event_thread, nullptr, _server_loop, this)) {
			Genode::error("could not create event thread");
			throw Init_failed();
		}

		/* add pipe to wake up loop on late connecting terminal */
		if (pipe(_server_fds) ||
			ssh_event_add_fd(_event_loop,
			                 _server_fds[0],
			                 POLLIN,
			                 write_avail_cb,
			                 this) != SSH_OK ) {
			Genode::error("Failed to create wakeup pipe");
			throw -1;
		}

		Genode::log("Listen on port: ", _port);
	}); /* Libc::with_libc */
}


Ssh::Server::~Server()
{
	close(_server_fds[0]);
	close(_server_fds[1]);
}


void Ssh::Server::_initialize_channel_callbacks()
{
	Genode::memset(&_channel_cb, 0, sizeof(_channel_cb));

	_channel_cb.userdata                           = this;
	_channel_cb.channel_data_function              = channel_data_cb;
	_channel_cb.channel_env_request_function       = channel_env_request_cb;
	_channel_cb.channel_pty_request_function       = channel_pty_request_cb;
	_channel_cb.channel_pty_window_change_function = channel_pty_window_change_cb;
	_channel_cb.channel_shell_request_function     = channel_shell_request_cb;
	_channel_cb.channel_exec_request_function      = channel_exec_request_cb;

	ssh_callbacks_init(&_channel_cb);
}


void Ssh::Server::_initialize_session_callbacks()
{
	Genode::memset(&_session_cb, 0, sizeof(_session_cb));

	_session_cb.userdata                              = this;
	_session_cb.auth_password_function                = session_auth_password_cb;
	_session_cb.auth_pubkey_function                  = session_auth_pubkey_cb;
	_session_cb.service_request_function              = session_service_request_cb;
	_session_cb.channel_open_request_session_function = session_channel_open_request_cb;

	ssh_callbacks_init(&_session_cb);
}


void Ssh::Server::_initialize_bind_callbacks()
{
	Genode::memset(&_bind_cb,  0, sizeof(_bind_cb));
	_bind_cb.incoming_connection = bind_incoming_connection;
	ssh_callbacks_init(&_bind_cb);
	ssh_bind_set_callbacks(_ssh_bind, &_bind_cb, this);
}


void Ssh::Server::_cleanup_session(Session &s)
{
	if (s.auth_sucessful) {
		_log_logout(s);
	}

	ssh_channel_free(s.channel);
	s.channel = nullptr;

	ssh_event_remove_session(_event_loop, s.session);
	ssh_disconnect(s.session);
	ssh_free(s.session);
	s.session = nullptr;

	if (s.terminal) {
		s.terminal->detach_channel();
	}

	try {
		_request_terminal_reporter.generate([&] (Xml_generator& xml) {
			xml.attribute("user", s.user());
			xml.attribute("exit", "now");
		});
	} catch (...) {
		Genode::warning("could not enable exit reporting");
	}

	Genode::destroy(&_heap, &s);
}


void Ssh::Server::_cleanup_sessions()
{
	auto cleanup = [&] (Session &s) {
		if (!ssh_is_connected(s.session)) {
			_cleanup_session(s);
		}
	};
	_sessions.for_each(cleanup);
}


void Ssh::Server::_parse_config(Genode::Xml_node const &config)
{
	using Util::Filename;

	_verbose    = config.attribute_value("verbose",    false);
	_log_level  = config.attribute_value("debug",      0u);
	_log_logins = config.attribute_value("log_logins", true);

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

	_rsa_key     = config.attribute_value("rsa_key",     Filename());
	_ecdsa_key   = config.attribute_value("ecdsa_key",   Filename());
	_ed25519_key = config.attribute_value("ed25519_key", Filename());

	Genode::log("Allowed auth methods: ",
	            _allow_password  ? "password "  : "",
	            _allow_publickey ? "public-key" : "");
}


void Ssh::Server::_load_hostkey(Util::Filename const &file)
{
	if (file.valid() &&
	    ssh_bind_options_set(_ssh_bind, SSH_BIND_OPTIONS_HOSTKEY,
	                         file.string()) < 0) {
		Genode::error("could not load hostkey '", file, "'");
		throw -1;
	}
}


void *Ssh::Server::_server_loop(void *arg)
{
	Ssh::Server *server = reinterpret_cast<Ssh::Server *>(arg);
	server->loop();
	return nullptr;
}


bool Ssh::Server::_allow_multi_login(ssh_session s, Login const &login)
{
	if (login.multi_login) { return true; }

	bool found = false;
	auto lookup = [&] (Session const &s) {
		if (s.user() == login.user) { found = true; }
	};
	_sessions.for_each(lookup);
	return !found;
}


void Ssh::Server::_log_failed(char const *user, Session const &s, bool pubkey)
{
	if (!_log_logins) { return; }

	char const *date = Util::get_time();
	Genode::log(date, " failed user ", user, " (", s.id(), ") ",
	            "with ", pubkey ? "public-key" : "password");
}


void Ssh::Server::_log_logout(Session const &s)
{
	if (!_log_logins) { return; }

	char const *date = Util::get_time();
	Genode::log(date, " logout user ", s.user(), " (", s.id(), ")");
}


void Ssh::Server::_log_login(User const &user, Session const &s, bool pubkey)
{
	if (!_log_logins) { return; }

	char const *date = Util::get_time();
	Genode::log(date, " login user ", user, " (", s.id(), ") ",
	            "with ", pubkey ? "public-key" : "password");
}


void Ssh::Server::attach_terminal(Ssh::Terminal &conn)
{
	Genode::Lock::Guard g(_terminals.lock());

	try {
		new (&_heap) Terminal_session(_terminals,
		                              conn, _event_loop);
	} catch (...) {
		Genode::error("could not attach Terminal for user ",
		              conn.user());
		throw -1;
	}

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

	_wake_loop();
}


void Ssh::Server::detach_terminal(Ssh::Terminal &conn)
{
	Genode::Lock::Guard g(_terminals.lock());

	Terminal_session *p = nullptr;
	auto lookup = [&] (Terminal_session &t) {

		if (&t.conn == &conn) {
			p = &t;
		}
	};
	_terminals.for_each(lookup);

	if (!p) {
		Genode::error("could not detach Terminal for user ", conn.user());
		return;
	}
	auto invalidate_terminal = [&] (Session &sess) {

		Libc::with_libc([&] () {
			ssh_blocking_flush(sess.session, 10000);
			ssh_disconnect(sess.session);
			if (sess.terminal != &conn) { return; }
			sess.terminal = nullptr;
		});
	};
	_sessions.for_each(invalidate_terminal);
	_cleanup_sessions();
	Genode::destroy(&_heap, p);
}


void Ssh::Server::update_config(Genode::Xml_node const &config)
{
	Genode::Lock::Guard g(_terminals.lock());

	_parse_config(config);
	ssh_bind_options_set(_ssh_bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &_log_level);
}


Ssh::Terminal *Ssh::Server::lookup_terminal(Session &s)
{
	Ssh::Terminal *p      = nullptr;
	auto           lookup = [&] (Terminal_session &t) {
		if (t.conn.user() == s.user()) { p = &t.conn; }
	};
	_terminals.for_each(lookup);
	return p;
}


Ssh::Session *Ssh::Server::lookup_session(ssh_session s)
{
	Session *p = nullptr;
	auto lookup = [&] (Session &sess) {
		if (sess.session == s) { p = &sess; }
	};
	_sessions.for_each(lookup);
	return p;
}


bool Ssh::Server::request_terminal(Session &session,
                                   const char* command)
{
	Genode::Lock::Guard g(_logins.lock());
	Login const *l = _logins.lookup(session.user().string());
	if (!l || !l->request_terminal) {
		return false;
	}

	try {
		_request_terminal_reporter.generate([&] (Xml_generator& xml) {
			xml.attribute("user", session.user());
			if (command) {
				xml.attribute("command", command);
			}
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


void Ssh::Server::incoming_connection(ssh_session s)
{
	/*
	 * In case we get bombarded by incoming connections, deny
	 * all attempts when this arbritray level is reached.
	 */
	enum { MEM_RESERVE = 128u * 1024, };
	if (_env.pd().avail_ram().value < (size_t)MEM_RESERVE) {
		error("Too many connections");
		throw -1;
	}

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
	int key_exchange_result = ssh_handle_key_exchange(s);

	if (SSH_OK != key_exchange_result) {
		Genode::warning("key exchange returned ", key_exchange_result);
	}

	ssh_event_add_session(_event_loop, s);
}


bool Ssh::Server::auth_password(ssh_session s, char const *u, char const *pass)
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


bool Ssh::Server::auth_pubkey(ssh_session s, char const *u,
                              struct ssh_key_struct *pubkey,
                              char signature_state)
{
	Session *p = lookup_session(s);
	if (!p || p->session != s) {
		Genode::warning("session not found");
		return false;
	}
	Session &session = *p;

	/*
	 * In this first state the given pubkey is solely probed.
	 * Ideally we would check here if the given pubkey is in fact to the
	 * configured one, i.e., reading a 'authorized_keys' like file and
	 * check its entries.
	 *
	 * For now we simple accept all keys and reject them in the later
	 * state.
	 */
	if (signature_state == SSH_PUBLICKEY_STATE_NONE) {
		return true;
	}

	/*
	 * In this second state we check the provided pubkey and if it
	 * matches allow authentication to proceed.
	 */
	if (signature_state == SSH_PUBLICKEY_STATE_VALID) {
		Genode::Lock::Guard g(_logins.lock());
		Login const *l = _logins.lookup(u);
		if (l && !ssh_key_cmp(pubkey, l->pub_key,
		                      SSH_KEY_CMP_PUBLIC)) {
			if (_allow_multi_login(s, *l)) {
				session.auth_sucessful = true;
				session.adopt(l->user);
				_log_login(l->user, session, true);
				return true;
			}
		}
	}

	_log_failed(u, session, true);
	return false;
}


void Ssh::Server::loop()
{
	while (true) {

		int const events = ssh_event_dopoll(_event_loop, -1);
		if (events == SSH_ERROR) {
			_cleanup_sessions();
		}

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


void Ssh::Server::_wake_loop()
{
	/* wake the event loop up */
	Libc::with_libc([&] {
		char c = 1;
		::write(_server_fds[1], &c, sizeof(c));
	});
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

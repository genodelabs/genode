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

#ifndef _SSH_TERMINAL_SERVER_H_
#define _SSH_TERMINAL_SERVER_H_

/* Genode includes */
#include <base/log.h>
#include <base/registry.h>
#include <os/reporter.h>

/* libc includes */
#include <poll.h>
#include <pthread.h>

/* libssh includes */
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

/* local includes */
#include "login.h"
#include "terminal.h"


namespace Ssh {

	using namespace Genode;

	struct Server;
	struct Session;
	struct Terminal_session;
	struct Terminal_registry;
}


struct Ssh::Session : Genode::Registry<Session>::Element
{
	User     _user { };
	uint32_t _id   { 0 };

	int  bad_auth_attempts { 0 };
	bool auth_sucessful    { false };

	ssh_session session              { nullptr };
	ssh_channel channel              { nullptr };
	ssh_channel_callbacks channel_cb { nullptr };

	Ssh::Terminal *terminal          { nullptr };

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


struct Ssh::Terminal_session : Genode::Registry<Terminal_session>::Element
{
	Ssh::Terminal &conn;

	ssh_event _event_loop;

	int _fds[2] { -1, -1 };

	Terminal_session(Genode::Registry<Terminal_session> &reg,
	                 Ssh::Terminal &conn,
	                 ssh_event event_loop);

	~Terminal_session()
	{
		ssh_event_remove_fd(_event_loop, _fds[0]);
		close(_fds[0]);
		close(_fds[1]);
	}
};


struct Ssh::Terminal_registry : Genode::Registry<Terminal_session>
{
	Genode::Lock _lock { };
	Genode::Lock &lock() { return _lock; }
};


class Ssh::Server
{
	public:

		struct Init_failed    : Genode::Exception { };
		struct Invalid_config : Genode::Exception { };

	private:

		using Session_registry = Genode::Registry<Session>;

		Genode::Env   &_env;
		Genode::Heap   _heap;

		bool           _verbose           { false };
		bool           _allow_password    { false };
		bool           _allow_publickey   { false };
		bool           _log_logins        { false };
		int            _max_auth_attempts { 3 };
		unsigned       _port              { 0u };
		unsigned       _log_level         { 0u };
		int            _server_fds[2]     { -1, -1 };

		bool           _config_once { false };

		ssh_bind       _ssh_bind;
		ssh_event      _event_loop;

		Util::Filename _rsa_key      { };
		Util::Filename _ecdsa_key    { };
		Util::Filename _ed25519_key  { };

		Expanding_reporter _request_terminal_reporter { _env,
		                                                "request_terminal",
		                                                "request_terminal" };

		Terminal_registry   _terminals { };
		Login_registry     &_logins;
		pthread_t           _event_thread;

		/*
		 * Since we always pass ourself as userdata pointer, we may
		 * safely use the same callback for all sessions and channels.
		 */
		ssh_channel_callbacks_struct _channel_cb { };
		ssh_server_callbacks_struct  _session_cb { };
		ssh_bind_callbacks_struct    _bind_cb    { };

		Session_registry _sessions   { };
		uint32_t         _session_id { 0 };

		void _initialize_channel_callbacks();
		void _initialize_session_callbacks();
		void _initialize_bind_callbacks();
		void _cleanup_session(Session &s);

		void _cleanup_sessions();
		void _parse_config(Genode::Xml_node const &config);
		void _load_hostkey(Util::Filename const &file);

		/*
		 * Event execution
		 */

		static void *_server_loop(void *arg);

		bool _allow_multi_login(ssh_session s, Login const &login);

		/********************
		 ** Login messages **
		 ********************/

		void _log_failed(char const *user, Session const &s, bool pubkey);
		void _log_logout(Session const &s);
		void _log_login(User const &user, Session const &s, bool pubkey);

		void _wake_loop();

	public:

		Server(Genode::Env &env,
		       Genode::Xml_node const &config,
		       Ssh::Login_registry    &logins);

		virtual ~Server();

		void loop();

		/***************************************************************
		 ** Methods below are only used by Terminal session front end **
		 ***************************************************************/

		/**
		 * Attach Terminal session
		 */
		void attach_terminal(Ssh::Terminal &conn);

		/**
		 * Detach Terminal session
		 */
		void detach_terminal(Ssh::Terminal &conn);

		/**
		 * Update config
		 */
		void update_config(Genode::Xml_node const &config);

		/*******************************************************
		 ** Methods below are only used by callback back ends **
		 *******************************************************/

		/**
		 * Look up Terminal for session
		 */
		Ssh::Terminal *lookup_terminal(Session &s);

		/**
		 * Look up Session for SSH session
		 */
		Session *lookup_session(ssh_session s);

		/**
		 * Request Terminal
		 */
		bool request_terminal(Session &session, const char* command = nullptr);

		/**
		 * Handle new incoming connections
		 */
		void incoming_connection(ssh_session s);

		/**
		 * Handle password authentication
		 */
		bool auth_password(ssh_session s, char const *u, char const *pass);

		/**
		 * Handle public-key authentication
		 */
		bool auth_pubkey(ssh_session s, char const *u,
		                 struct ssh_key_struct *pubkey,
                         char signature_state);
};

#endif /* _SSH_TERMINAL_SERVER_H_ */

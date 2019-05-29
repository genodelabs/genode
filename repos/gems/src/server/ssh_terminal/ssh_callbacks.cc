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


/* libssh includes */
#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

/* local includes */
#include "server.h"


/***********************
 ** Channel callbacks **
 ***********************/

/**
 * Handle SSH channel data request
 */
int channel_data_cb(ssh_session session, ssh_channel channel,
                    void *data, uint32_t len, int is_stderr,
                    void *userdata)
{
	using Genode::error;
	using Genode::Lock;

	if (len == 0) {
		return 0;
	}

	Ssh::Server  &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Session *p      = server.lookup_session(session);
	if (!p) {
		error("session not found");
		return SSH_ERROR;
	}

	if (p->channel != channel) {
		error("wrong channel");
		return SSH_ERROR;
	}

	if (!p->terminal) {
		error("no terminal");
		return SSH_ERROR;
	}

	Ssh::Terminal &conn      { *p->terminal };
	Lock::Guard    guard     { conn.read_buf.lock() };
	char const    *src       { reinterpret_cast<char const*>(data) };
	size_t         num_bytes { 0 };

	while ((conn.read_buf.write_avail() > 0) && (num_bytes < len)) {

		char c = src[num_bytes];

		/* replace ^? with ^H and let's hope we do not break anything */
		enum { DEL = 0x7f, BS = 0x08, };
		if (c == DEL) {
			conn.read_buf.append(BS);
		} else {
			conn.read_buf.append(c);
		}

		num_bytes++;
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
int channel_env_request_cb(ssh_session session, ssh_channel channel,
                           char const *env_name, char const *env_value,
                           void *userdata)
{
	return SSH_OK;
}


/**
 * Handle SSH channel PTY request
 */
int channel_pty_request_cb(ssh_session session, ssh_channel channel,
                           char const *term,
                           int cols, int rows, int py, int px,
                           void *userdata)
{
	using namespace Genode;
	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Session *p = server.lookup_session(session);
	if (!p || p->channel != channel) { return SSH_ERROR; }

	/*
	 * Look up terminal and in case there is none, check
	 * if we have to wait for another subsystem to come up.
	 * In this case we return successfully to the client
	 * and wait for a Terminal session to be established.
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
int channel_pty_window_change_cb(ssh_session session, ssh_channel channel,
                                 int width, int height, int pxwidth, int pwheight,
                                 void *userdata)
{
	(void)pxwidth;
	(void)pwheight;

	using namespace Genode;
	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Session *p = server.lookup_session(session);
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
int channel_shell_request_cb(ssh_session session, ssh_channel channel,
                             void *userdata)
{
	return SSH_OK;
}


/**
 * Handle SSH channel exec request
 *
 * Exec requests provide a command that needs to be executed.
 * The command is provided while starting a new terminal using
 * request_terminal().
 */
int channel_exec_request_cb(ssh_session session, ssh_channel channel,
                            const char *command,
                            void *userdata)
{
	using namespace Genode;

	Ssh::Server  &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Session *p      = server.lookup_session(session);
	if (!p || p->channel != channel) { return SSH_ERROR; }

	/*
	 * Look up terminal and in case there is none, check
	 * if we have to wait for another subsystem to come up.
	 * In this case we return successfully to the client
	 * and wait for a Terminal session to be established.
	 */
	if (!p->terminal) {
		p->terminal = server.lookup_terminal(*p);
		if (!p->terminal) {
			return server.request_terminal(*p, command) ? SSH_OK
			                                            : SSH_ERROR;
		}
	}
	/* exec commands can only be done with newly started terminals */
	return SSH_ERROR;
}


/***********************
 ** Session callbacks **
 ***********************/

/**
 * Handle SSH session service requests
 */
int session_service_request_cb(ssh_session,
                               char const *service, void*)
{
	return Genode::strcmp(service, "ssh-userauth") == 0 ? 0 : -1;
}


/**
 * Handle SSH session password authentication requests
 */
int session_auth_password_cb(ssh_session session,
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
int session_auth_pubkey_cb(ssh_session session, char const *user,
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
ssh_channel session_channel_open_request_cb(ssh_session session,
                                            void *userdata)
{
	using namespace Genode;

	Ssh::Server  &server = *reinterpret_cast<Ssh::Server*>(userdata);
	Ssh::Session *p      = server.lookup_session(session);
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
void bind_incoming_connection(ssh_bind sshbind, void *userdata)
{
	using namespace Genode;

	ssh_session session = ssh_new();
	if (!session || ssh_bind_accept(sshbind, session)) {
		error("could not accept session: '", ssh_get_error(session), "'");
		ssh_free(session);
		return;
	}

	Ssh::Server &server = *reinterpret_cast<Ssh::Server*>(userdata);
	try {
		server.incoming_connection(session);
	}
	catch (...) {
		ssh_disconnect(session);
		ssh_free(session);
	}
}

/*
 * \brief  Connection to Nitpicker service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NITPICKER_SESSION__CONNECTION_H_
#define _INCLUDE__NITPICKER_SESSION__CONNECTION_H_

#include <nitpicker_session/client.h>
#include <framebuffer_session/client.h>
#include <input_session/client.h>
#include <util/arg_string.h>
#include <base/connection.h>

namespace Nitpicker { class Connection; }


class Nitpicker::Connection : public Genode::Connection<Session>,
                              public Session_client
{
	public:

		enum { RAM_QUOTA = 36*1024UL };

	private:

		Framebuffer::Session_client _framebuffer;
		Input::Session_client       _input;
		Genode::size_t              _session_quota = 0;

		/**
		 * Create session and return typed session capability
		 */
		Session_capability _connect(Genode::Parent &parent, char const *label)
		{
			enum { ARGBUF_SIZE = 128 };
			char argbuf[ARGBUF_SIZE];
			argbuf[0] = 0;

			if (Genode::strlen(label) > 0)
				Genode::snprintf(argbuf, sizeof(argbuf), "label=\"%s\"", label);

			/*
			 * Declare ram-quota donation
			 */
			using Genode::Arg_string;
			Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", RAM_QUOTA);

			return session(parent, argbuf);
		}

	public:

		/**
		 * Constructor
		 */
		Connection(Genode::Env &env, char const *label = "")
		:
			/* establish nitpicker session */
			Genode::Connection<Session>(env, _connect(env.parent(), label)),
			Session_client(env.rm(), cap()),

			/* request frame-buffer and input sub sessions */
			_framebuffer(framebuffer_session()),
			_input(env.rm(), input_session())
		{ }

		/**
		 * Constructor
		 *
		 * \noapi
		 * \deprecated  Use the constructor with 'Env &' as first
		 *              argument instead
		 */
		Connection(char const *label = "") __attribute__((deprecated))
		:
			/* establish nitpicker session */
			Genode::Connection<Session>(_connect(*Genode::env_deprecated()->parent(), label)),
			Session_client(*Genode::env_deprecated()->rm_session(), cap()),

			/* request frame-buffer and input sub sessions */
			_framebuffer(framebuffer_session()),
			_input(*Genode::env_deprecated()->rm_session(), input_session())
		{ }

		void buffer(Framebuffer::Mode mode, bool use_alpha)
		{
			Genode::size_t const needed = ram_quota(mode, use_alpha);
			Genode::size_t const upgrade = needed > _session_quota
			                             ? needed - _session_quota
			                             : 0;
			if (upgrade > 0) {
				this->upgrade_ram(upgrade);
				_session_quota += upgrade;
			}

			Session_client::buffer(mode, use_alpha);
		}

		/**
		 * Return sub session for Nitpicker's input service
		 */
		Input::Session_client *input() { return &_input; }

		/**
		 * Return sub session for session's frame buffer
		 */
		Framebuffer::Session *framebuffer() { return &_framebuffer; }
};

#endif /* _INCLUDE__NITPICKER_SESSION__CONNECTION_H_ */

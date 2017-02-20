/*
 * \brief  Connection to frame-buffer service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_
#define _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_

#include <framebuffer_session/client.h>
#include <util/arg_string.h>
#include <base/connection.h>

namespace Framebuffer { class Connection; }


class Framebuffer::Connection : public Genode::Connection<Session>,
                                public Session_client
{
	public:

		enum { RAM_QUOTA = 8*1024UL };

	private:

		/**
		 * Create session and return typed session capability
		 */
		Session_capability _connect(Genode::Parent &parent,
		                            unsigned width, unsigned height,
		                            Mode::Format format)
		{
			using namespace Genode;

			enum { ARGBUF_SIZE = 128 };
			char argbuf[ARGBUF_SIZE];
			argbuf[0] = 0;

			/* donate ram quota for storing server-side meta data */
			Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", RAM_QUOTA);

			/* set optional session-constructor arguments */
			if (width)
				Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_width", width);
			if (height)
				Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_height", height);
			if (format != Mode::INVALID)
				Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_format", format);

			return session(parent, argbuf);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param mode  desired size and pixel format
		 *
		 * The specified values are not enforced. After creating the
		 * session, you should validate the actual frame-buffer attributes
		 * by calling the 'info' method of the frame-buffer interface.
		 */
		Connection(Genode::Env &env, Framebuffer::Mode mode)
		:
			Genode::Connection<Session>(env, _connect(env.parent(),
			                                          mode.width(), mode.height(),
			                                          mode.format())),
			Session_client(cap())
		{ }

		/**
		 * Constructor
		 *
		 * \noapi
		 * \deprecated  Use the constructor with 'Env &' as first
		 *              argument instead
		 */
		Connection(unsigned     width  = 0,
		           unsigned     height = 0,
		           Mode::Format format = Mode::INVALID) __attribute__((deprecated))
		:
			Genode::Connection<Session>(_connect(*Genode::env_deprecated()->parent(),
			                                     width, height, format)),
			Session_client(cap())
		{ }
};

#endif /* _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_ */

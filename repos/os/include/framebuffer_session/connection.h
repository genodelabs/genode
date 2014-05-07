/*
 * \brief  Connection to frame-buffer service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_
#define _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_

#include <framebuffer_session/client.h>
#include <util/arg_string.h>
#include <base/connection.h>

namespace Framebuffer {

	class Connection : public Genode::Connection<Session>,
	                   public Session_client
	{
		private:

			/**
			 * Create session and return typed session capability
			 */
			Session_capability _connect(unsigned width, unsigned height,
			                            Mode::Format format)
			{
				using namespace Genode;

				enum { ARGBUF_SIZE = 128 };
				char argbuf[ARGBUF_SIZE];

				/* donate ram quota for storing server-side meta data */
				Genode::strncpy(argbuf, "ram_quota=8K", sizeof(argbuf));

				/* set optional session-constructor arguments */
				if (width)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_width", width);
				if (height)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_height", height);
				if (format != Mode::INVALID)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_format", format);

				return session(argbuf);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param width   desired frame-buffer width
			 * \param height  desired frame-buffer height
			 * \param mode    desired pixel format
			 *
			 * The specified values are not enforced. After creating the
			 * session, you should validate the actual frame-buffer attributes
			 * by calling the 'info' function of the frame-buffer interface.
			 */
			Connection(unsigned     width  = 0,
			           unsigned     height = 0,
			           Mode::Format format = Mode::INVALID)
			:
				Genode::Connection<Session>(_connect(width, height, format)),
				Session_client(cap())
			{ }
	};
}

#endif /* _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_ */

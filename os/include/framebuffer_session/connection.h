/*
 * \brief  Connection to frame-buffer service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2011 Genode Labs GmbH
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
			Session_capability _connect(unsigned width, unsigned height, Mode mode)
			{
				using namespace Genode;

				enum { ARGBUF_SIZE = 128 };
				char argbuf[ARGBUF_SIZE];

				/* donate ram quota for storing server-side meta data */
				strncpy(argbuf, "ram_quota=8K", sizeof(argbuf));

				/* set optional session-constructor arguments */
				if (width)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_width", width);
				if (height)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_height", height);
				if (mode != Session::INVALID)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_mode", mode);

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
			Connection(unsigned  width  = 0,
			           unsigned  height = 0,
			           Mode      mode   = INVALID)
			:
				Genode::Connection<Session>(_connect(width, height, mode)),
				Session_client(cap())
			{ }
	};
}

#endif /* _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_ */

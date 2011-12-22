/*
 * \brief  Connection to Nitpicker service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_SESSION__CONNECTION_H_
#define _INCLUDE__NITPICKER_SESSION__CONNECTION_H_

#include <nitpicker_session/client.h>
#include <framebuffer_session/client.h>
#include <input_session/client.h>
#include <util/arg_string.h>
#include <base/connection.h>

namespace Nitpicker {

	class Connection : public Genode::Connection<Session>,
	                   public Session_client
	{
		private:

			Framebuffer::Session_client _framebuffer;
			Input::Session_client       _input;

			/**
			 * Create session and return typed session capability
			 */
			Session_capability
			_connect(unsigned width, unsigned height, bool alpha,
			         Framebuffer::Session::Mode mode, bool stay_top)
			{
				using namespace Genode;

				enum { ARGBUF_SIZE = 128 };
				char argbuf[ARGBUF_SIZE];
				argbuf[0] = 0;

				/* by default, donate as much as needed for a 1024x768 RGB565 screen */
				Genode::size_t ram_quota = 1600*1024;

				/*
				 * NOTE: When specifying an INVALID mode as argument, we could
				 * probe for any valid video mode. For now, we just probe for
				 * RGB565.
				 */
				if (mode == Framebuffer::Session::INVALID)
					mode =  Framebuffer::Session::RGB565;

				/* if buffer dimensions are specified, calculate ram quota to donate */
				if (width && height)
					ram_quota = width*height*Framebuffer::Session::bytes_per_pixel(mode);

				/* account for alpha and input-mask buffers */
				if (alpha)
					ram_quota += width*height*2;

				/* add quota for storing server-side meta data */
				enum { SESSION_METADATA = 16*1024 };
				ram_quota += SESSION_METADATA;

				/* declare ram-quota donation */
				Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", ram_quota);

				/* set optional session-constructor arguments */
				if (width)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_width", width);
				if (height)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_height", height);
				if (mode != Framebuffer::Session::INVALID)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "fb_mode", mode);
				if (alpha)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "alpha", "yes");
				if (stay_top)
					Arg_string::set_arg(argbuf, sizeof(argbuf), "stay_top", "yes");

				return session(argbuf);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param width   desired buffer width
			 * \param height  desired buffer height
			 * \param alpha   true for using a buffer with alpha channel
			 * \param mode    desired pixel format
			 *
			 * The specified value for 'mode' is not enforced. After creating the
			 * session, you should validate the actual pixel format of the
			 * buffer by its 'info'.
			 */
			Connection(unsigned width  = 0, unsigned height = 0, bool alpha = false,
			           Framebuffer::Session::Mode mode = Framebuffer::Session::INVALID,
			           bool stay_top = false)
			:
				/* establish nitpicker session */
				Genode::Connection<Session>(_connect(width, height, alpha, mode, stay_top)),
				Session_client(cap()),

				/* request frame-buffer and input sub sessions */
				_framebuffer(framebuffer_session()),
				_input(input_session())
			{ }

			/**
			 * Return sub session for Nitpicker's input service
			 */
			Input::Session *input() { return &_input; }

			/**
			 * Return sub session for session's frame buffer
			 */
			Framebuffer::Session *framebuffer() { return &_framebuffer; }
	};
}

#endif /* _INCLUDE__NITPICKER_SESSION__CONNECTION_H_ */

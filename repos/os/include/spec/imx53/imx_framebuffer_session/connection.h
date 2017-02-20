/*
 * \brief  Connection to i.MX53 specific frame-buffer service
 * \author Stefan Kalkowski
 * \date   2013-02-26
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IMX_FRAMEBUFFER_SESSION__CONNECTION_H_
#define _INCLUDE__IMX_FRAMEBUFFER_SESSION__CONNECTION_H_

#include <imx_framebuffer_session/client.h>
#include <util/arg_string.h>
#include <base/connection.h>

namespace Framebuffer { class Imx_connection; }


class Framebuffer::Imx_connection : public Genode::Connection<Imx_session>,
                                    public Imx_client
{
	private:

		/**
		 * Create session and return typed session capability
		 */
		Capability<Imx_session> _connect(Genode::Parent &parent,
		                                 unsigned width, unsigned height,
		                                 Mode::Format format)
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
		Imx_connection(Genode::Env &env, Framebuffer::Mode mode)
		:
			Genode::Connection<Imx_session>(env, _connect(env.parent(),
			                                              mode.width(), mode.height(),
			                                              mode.format())),
			Imx_client(cap())
		{ }

		/**
		 * Constructor
		 *
		 * \noapi
		 * \deprecated  Use the constructor with 'Env &' as first
		 *              argument instead
		 */
		Imx_connection(unsigned     width  = 0,
		               unsigned     height = 0,
		               Mode::Format format = Mode::INVALID) __attribute__((deprecated))
		: Genode::Connection<Imx_session>(_connect(*Genode::env_deprecated()->parent(),
		                                           width, height, format)),
		  Imx_client(cap()) { }
};

#endif /* _INCLUDE__IMX_FRAMEBUFFER_SESSION__CONNECTION_H_ */

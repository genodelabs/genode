/*
 * \brief  Connection to frame-buffer service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_
#define _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_

#include <framebuffer_session/client.h>
#include <base/connection.h>

namespace Framebuffer { class Connection; }


struct Framebuffer::Connection : Genode::Connection<Session>, Session_client
{
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
		Genode::Connection<Session>(env, Label(), Ram_quota { 8*1024 },
		                            Args("fb_width=",  mode.area.w(), ", "
		                                 "fb_height=", mode.area.h())),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__FRAMEBUFFER_SESSION__CONNECTION_H_ */

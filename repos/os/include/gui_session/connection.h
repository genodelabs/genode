/*
 * \brief  Connection to GUI service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GUI_SESSION__CONNECTION_H_
#define _INCLUDE__GUI_SESSION__CONNECTION_H_

#include <gui_session/client.h>
#include <framebuffer_session/client.h>
#include <input_session/client.h>
#include <base/connection.h>

namespace Gui { class Connection; }


class Gui::Connection : public Genode::Connection<Session>,
                        public Session_client
{
	private:

		Framebuffer::Session_client _framebuffer;
		Input::Session_client       _input;
		Genode::size_t              _session_quota = 0;

	public:

		/**
		 * Constructor
		 */
		Connection(Genode::Env &env, Label const &label = Label())
		:
			/* establish nitpicker session */
			Genode::Connection<Session>(env, label, Ram_quota { 36*1024 }, Args()),
			Session_client(env.rm(), cap()),

			/* request frame-buffer and input sub sessions */
			_framebuffer(framebuffer_session()),
			_input(env.rm(), input_session())
		{ }

		void buffer(Framebuffer::Mode mode, bool use_alpha) override
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
		 * Return sub session for GUI's input service
		 */
		Input::Session_client *input() { return &_input; }

		/**
		 * Return sub session for session's frame buffer
		 */
		Framebuffer::Session *framebuffer() { return &_framebuffer; }
};

#endif /* _INCLUDE__GUI_SESSION__CONNECTION_H_ */

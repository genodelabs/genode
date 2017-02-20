/*
 * \brief  Manager of all VM requested console functionality
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

/* base includes */
#include <base/env.h>
#include <dataspace/client.h>
#include <util/reconstructible.h>
#include <util/string.h>

/* os includes */
#include <framebuffer_session/connection.h>
#include <input/event.h>
#include <input_session/connection.h>
#include <timer_session/connection.h>

#include <os/pixel_rgb565.h>

/* local includes */
#include "keyboard.h"
#include "synced_motherboard.h"

namespace Seoul {
	class Console;
}

class Seoul::Console : public StaticReceiver<Seoul::Console>
{
	private:

		Genode::Env                 &_env;
		Motherboard                 &_unsynchronized_motherboard;
		Synced_motherboard          &_motherboard;
		Genode::Constructible<Framebuffer::Connection> _framebuffer;
		Genode::Constructible<Genode::Surface<Genode::Pixel_rgb565> > _surface;
		Input::Connection            _input = { _env };
		Keyboard                     _vkeyb = { _motherboard };
		short                       *_pixels   = nullptr;
		char                        *_guest_fb = nullptr;
		unsigned long                _fb_size  = 0;
		Genode::Dataspace_capability _fb_ds;
		Genode::size_t               _vm_fb_size;
		VgaRegs                     *_regs     = nullptr;
		Framebuffer::Mode            _fb_mode;
		bool                         _left     = false;
		bool                         _middle   = false;
		bool                         _right    = false;
		unsigned                     _timer;

		unsigned _input_to_ps2mouse(Input::Event const &);

		Genode::Signal_handler<Console> _signal_input
			= { _env.ep(), *this, &Console::_handle_input };

		void _handle_input();
		unsigned _handle_fb();

	public:

		/* bus callbacks */
		bool receive(MessageConsole &);
		bool receive(MessageMemRegion &);
		bool receive(MessageTimeout &);

		void register_host_operations(Motherboard &);

		/**
		 * Constructor
		 */
		Console(Genode::Env &env, Synced_motherboard &, Motherboard &,
		        Genode::size_t vm_fb_size, Genode::Dataspace_capability fb_ds);
};

#endif /* _CONSOLE_H_ */

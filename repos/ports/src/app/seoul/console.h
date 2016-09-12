/*
 * \brief  Manager of all VM requested console functionality
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013 Genode Labs GmbH
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

/* Genode includes */
#include <util/string.h>
#include <framebuffer_session/connection.h>
#include <input_session/connection.h>
#include <timer_session/connection.h>
#include <dataspace/client.h>

/* local includes */
#include "synced_motherboard.h"

/* includes for I/O */
#include <base/env.h>
#include <util/list.h>
#include <input/event.h>

using Genode::List;
using Genode::Thread_deprecated;

class Vancouver_console : public Thread_deprecated<8192>, public StaticReceiver<Vancouver_console>
{
	private:

		Genode::Lock                 _startup_lock;
		Synced_motherboard          &_motherboard;
		Genode::Lock                 _console_lock;
		short                       *_pixels;
		char                        *_guest_fb;
		unsigned long                _fb_size;
		Genode::Dataspace_capability _fb_ds;
		Genode::size_t               _vm_fb_size;
		VgaRegs                     *_regs;
		Framebuffer::Mode            _fb_mode;
		bool                         _left, _middle, _right;

		unsigned _input_to_ps2mouse(Input::Event const &);

	public:

		/* bus callbacks */
		bool receive(MessageConsole &msg);
		bool receive(MessageMemRegion &msg);

		void register_host_operations(Motherboard &);

		/* initialisation */
		void entry();

		/**
		 * Constructor
		 */
		Vancouver_console(Synced_motherboard &,
		                  Genode::size_t vm_fb_size,
		                  Genode::Dataspace_capability fb_ds);
};

#endif /* _CONSOLE_H_ */

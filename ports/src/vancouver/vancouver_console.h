/*
 * \brief  Manager of all VM requested console functionality
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation    
 * 
 * This file is part of the Genode OS framework and being contributed under
 * the terms and conditions of the GNU General Public License version 2.   
 */

#ifndef _VANCOUVER_CONSOLE_H
#define _VANCOUVER_CONSOLE_H

/* Genode includes */
#include <util/string.h>
#include <base/sleep.h>
#include <framebuffer_session/connection.h>
#include <input_session/connection.h>
#include <timer_session/connection.h>
#include <dataspace/client.h>

/* NOVA userland includes */
#include <nul/motherboard.h>

/* Includes for I/O */
#include <base/env.h>
#include <util/list.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_view/client.h>
#include <input/event.h>

/* Graphics */
using Genode::List;
using Genode::Thread;

class Vancouver_console : public Thread<8192>, public StaticReceiver<Vancouver_console> {

	private:
		Motherboard                 &_mb;
		short	                    *_pixels;
		char 	                    *_guest_fb;
		unsigned long                _fb_size;
		Genode::Dataspace_capability _fb_ds;
		Genode::size_t               _vm_fb_size;
		VgaRegs	                    *_regs;
		Framebuffer::Mode            _fb_mode;

	public:
		/* Bus Callbacks */
		bool receive(MessageConsole &msg);
		bool receive(MessageMemRegion &msg);

		/* Initialisation */
		void entry();

		/*
		 * Constructor
		 */
		Vancouver_console(Motherboard &mb, Genode::size_t vm_fb_size, Genode::Dataspace_capability fb_ds);
};

#endif

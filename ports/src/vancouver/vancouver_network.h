/*
 * \brief  Network receive handler per MAC address
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation    
 * 
 * This file is part of the Genode OS framework and being contributed under
 * the terms and conditions of the GNU General Public License version 2.   
 */

#ifndef _VANCOUVER_NETWORK_H
#define _VANCOUVER_NETWORK_H

/* Genode includes */
#include <util/list.h>
#include <util/string.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <base/allocator_avl.h>
#include <nic_session/connection.h>

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

class Vancouver_network : public Thread<4096> {
	private:
		Motherboard  &_mb;
		Nic::Session *_nic;

	public:
		/* Initialisation */
		void entry();

		/*
		 * Constructor
		 */
		Vancouver_network(Motherboard &mb, Nic::Session *nic);
};

#endif

/*
 * \brief  Network receive handler per MAC address
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

#ifndef _NETWORK_H_
#define _NETWORK_H_

/* Genode includes */
#include <nic_session/connection.h>

/* local includes */
#include "synced_motherboard.h"

using Genode::List;
using Genode::Thread_deprecated;

class Vancouver_network : public Thread_deprecated<4096>
{
	private:

		Synced_motherboard &_motherboard;
		Nic::Session       *_nic;

	public:

		/* initialisation */
		void entry();

		/**
		 * Constructor
		 */
		Vancouver_network(Synced_motherboard &, Nic::Session *);
};

#endif /* _NETWORK_H_ */

/*
 * \brief  Main-signal receiver and signal-helper functions
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <base/env.h>
#include <base/printf.h>
#include <base/signal.h>
#include <os/server.h>

static bool const verbose = false;

/**
 * Helper that holds sender and entrypoint
 */
class Signal_helper
{
	private:

		Server::Entrypoint        &_ep;
		Genode::Signal_transmitter _sender;

	public:

		Signal_helper(Server::Entrypoint &ep) : _ep(ep) { }

		Server::Entrypoint         &ep()     { return _ep;      }
		Genode::Signal_transmitter &sender() { return _sender; }
};



namespace Storage
{
	void init(Server::Entrypoint &ep);
}

namespace Nic
{
	void init(Server::Entrypoint &ep);
}

namespace Raw
{
	void init(Server::Entrypoint &ep, bool report_device_list);
}

#endif /* _SIGNAL_H_ */

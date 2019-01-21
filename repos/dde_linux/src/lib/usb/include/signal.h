/*
 * \brief  Main-signal receiver and signal-helper functions
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <platform.h>

#include <base/signal.h>

static bool const verbose = false;

/**
 * Helper that holds sender and entrypoint
 */
class Signal_helper
{
	private:

		Genode::Env                &_env;
		Genode::Signal_transmitter  _sender;

	public:

		Signal_helper(Genode::Env &env) : _env(env) { }

		Genode::Entrypoint         &ep()     { return _env.ep(); }
		Genode::Signal_transmitter &sender() { return _sender;   }
		Genode::Parent             &parent() { return _env.parent(); } 
		Genode::Env                &env()    { return _env; }
		Genode::Ram_allocator      &ram()    { return _env.ram(); }
		Genode::Region_map         &rm()     { return _env.rm(); }
};


namespace Storage
{
	void init(Genode::Env &env);
}

namespace Nic
{
	void init(Genode::Env &env);
}

namespace Raw
{
	void init(Genode::Env &env, bool report_device_list);
}


#endif /* _SIGNAL_H_ */

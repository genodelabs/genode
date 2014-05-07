/*
 * \brief  Front end of the partition server
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <block_session/rpc_object.h>
#include <cap_session/connection.h>
#include <os/config.h>

#include "driver.h"
#include "partition_table.h"
#include "component.h"

static Genode::Signal_receiver receiver;

Block::Driver& Block::Driver::driver()
{
	static Block::Driver driver(receiver);
	return driver;
}


void Block::Driver::_ready_to_submit(unsigned) {
	Block::Session_component::wake_up(); }


int main()
{
	using namespace Genode;

	if (!Block::Partition_table::table().avail()) {
		PERR("No valid partition table found");
		return 1;
	}

	enum { STACK_SIZE = 1024 * sizeof(Genode::size_t) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "part_ep");
	static Block::Root block_root(&ep, env()->heap(), receiver);

	env()->parent()->announce(ep.manage(&block_root));

	while (true) {
		Signal s = receiver.wait_for_signal();
		static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
	}

	return 0;
}

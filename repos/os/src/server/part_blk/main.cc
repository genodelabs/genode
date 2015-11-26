/*
 * \brief  Front end of the partition server
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <block_session/rpc_object.h>
#include <cap_session/connection.h>
#include <os/config.h>

#include "component.h"
#include "driver.h"
#include "gpt.h"
#include "mbr.h"

static Genode::Signal_receiver receiver;

Block::Driver& Block::Driver::driver()
{
	static Block::Driver driver(receiver);
	return driver;
}


void Block::Driver::_ready_to_submit(unsigned) {
	Block::Session_component::wake_up(); }


static bool _use_gpt()
{
	try {
		return Genode::config()->xml_node().attribute("use_gpt").has_value("yes");
	} catch(...) { }

	return false;
}


int main()
{
	using namespace Genode;

	bool valid_mbr = false;
	bool valid_gpt = false;
	bool use_gpt   = _use_gpt();

	if (use_gpt)
		try { valid_gpt = Gpt::table().avail(); } catch (...) { }

	/* fall back to MBR */
	if (!valid_gpt) {
		try { valid_mbr = Mbr_partition_table::table().avail(); }
		catch (Mbr_partition_table::Protective_mbr_found) {
			if (!use_gpt)
				PERR("Aborting: found protective MBR but GPT usage was not requested.");
			return 1;
		}
	}

	Block::Partition_table *partition_table = 0;
	if (valid_gpt)
		partition_table = &Gpt::table();
	if (valid_mbr)
		partition_table = &Mbr_partition_table::table();

	if (!partition_table) {
		PERR("Aborting: no partition table found.");
		return 1;
	}

	enum { STACK_SIZE = 2048 * sizeof(Genode::size_t) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "part_ep");
	static Block::Root block_root(&ep, env()->heap(), receiver,
	                              *partition_table);

	env()->parent()->announce(ep.manage(&block_root));

	while (true) {
		Signal s = receiver.wait_for_signal();
		static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
	}

	return 0;
}

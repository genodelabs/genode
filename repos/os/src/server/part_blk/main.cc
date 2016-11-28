/*
 * \brief  Front end of the partition server
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/attached_rom_dataspace.h>
#include <block_session/rpc_object.h>

#include "component.h"
#include "driver.h"
#include "gpt.h"
#include "mbr.h"


void Block::Driver::_ready_to_submit() {
	Block::Session_component::wake_up(); }


class Main
{
	private:

		Block::Partition_table & _table();

		Genode::Env &       _env;
		Genode::Heap        _heap   { _env.ram(), _env.rm() };
		Block::Driver       _driver { _env.ep(), _heap      };
		Mbr_partition_table _mbr    { _heap, _driver        };
		Gpt                 _gpt    { _heap, _driver        };
		Block::Root         _root   { _env, _heap, _driver, _table() };

	public:

		class No_partion_table : Genode::Exception {};

		Main(Genode::Env &env) : _env(env)
		{
			/*
			 * we read all partition information,
			 * now it's safe to turn in asynchronous mode
			 */
			_driver.work_asynchronously();

			/* announce at parent */
			env.parent().announce(env.ep().manage(_root));
		}
};


Block::Partition_table & Main::_table()
{
	bool valid_mbr = false;
	bool valid_gpt = false;
	bool use_gpt   = false;

	try {
		Genode::Attached_rom_dataspace config(_env, "config");
		use_gpt = config.xml().attribute_value("use_gpt", false);
	} catch(...) {}

	if (use_gpt)
		try { valid_gpt = _gpt.parse(); } catch (...) { }

	/* fall back to MBR */
	if (!valid_gpt) {
		try { valid_mbr = _mbr.parse(); }
		catch (Mbr_partition_table::Protective_mbr_found) {
			if (!use_gpt)
				Genode::error("Aborting: found protective MBR but ",
				              "GPT usage was not requested.");
			throw;
		}
	}

	if (valid_gpt) return _gpt;
	if (valid_mbr) return _mbr;

	Genode::error("Aborting: no partition table found.");
	throw No_partion_table();
}


void Component::construct(Genode::Env &env) { static Main main(env); }

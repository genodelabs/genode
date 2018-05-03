/*
 * \brief  Front end of the partition server
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
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

		Genode::Env &_env;

		Genode::Attached_rom_dataspace _config { _env, "config" };

		Genode::Heap        _heap     { _env.ram(), _env.rm() };
		Block::Driver       _driver   { _env, _heap      };
		Genode::Reporter    _reporter { _env, "partitions" };
		Mbr_partition_table _mbr      { _heap, _driver, _reporter };
		Gpt                 _gpt      { _heap, _driver, _reporter };
		Block::Root         _root     { _env, _config.xml(), _heap, _driver, _table() };

	public:

		struct No_partion_table : Genode::Exception { };
		struct Ambiguous_tables : Genode::Exception { };
		struct Invalid_config   : Genode::Exception { };

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
	using namespace Genode;

	bool valid_mbr  = false;
	bool valid_gpt  = false;
	bool ignore_gpt = false;
	bool ignore_mbr = false;
	bool pmbr_found = false;
	bool report     = false;

	try {
		ignore_gpt = _config.xml().attribute_value("ignore_gpt", false);
		ignore_mbr = _config.xml().attribute_value("ignore_mbr", false);
	} catch(...) {}

	if (ignore_gpt && ignore_mbr) {
		error("invalid configuration: cannot ignore GPT as well as MBR");
		throw Invalid_config();
	}

	try {
		report = _config.xml().sub_node("report").attribute_value
                         ("partitions", false);
		if (report)
			_reporter.enabled(true);
	} catch(...) {}

	/*
	 * Try to parse MBR as well as GPT first if not instructued
	 * to ignore either one of them.
	 */

	if (!ignore_mbr) {
		try { valid_mbr = _mbr.parse(); }
		catch (Mbr_partition_table::Protective_mbr_found) {
			pmbr_found = true;
		}
	}

	if (!ignore_gpt) {
		try { valid_gpt = _gpt.parse(); }
		catch (...) { }
	}

	/*
	 * Both tables are valid (although we would have expected a PMBR in
	 * conjunction with a GPT header - hybrid operation is not supported)
	 * and we will not decide which one to use, it is up to the user.
	 */

	if (valid_mbr && valid_gpt) {
		error("ambigious tables: found valid MBR as well as valid GPT");
		throw Ambiguous_tables();
	}

	if (valid_gpt && !pmbr_found) {
		warning("will use GPT without proper protective MBR");
	}

	/* PMBR missing, i.e, MBR part[0] contains whole disk and GPT valid */
	if (pmbr_found && ignore_gpt) {
		warning("found protective MBR but GPT is to be ignored");
	}

	/*
	 * Return the appropriate table or abort if none is found.
	 */

	if (valid_gpt) return _gpt;
	if (valid_mbr) return _mbr;

	error("Aborting: no partition table found.");
	throw No_partion_table();
}


void Component::construct(Genode::Env &env) { static Main main(env); }

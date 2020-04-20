/*
 * \brief  Test for combining vfs/ram, fs_rom, and fs_report
 * \author Norman Feske
 * \date   2017-06-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <vfs/simple_env.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <timer_session/connection.h>


namespace Test {
	struct Main;
	using namespace Genode;
}


struct Test::Main
{
	Env &_env;

	Genode::Heap _heap { _env.pd(), _env.rm() };

	Timer::Connection _timer { _env };

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	Genode::Xml_node vfs_config()
	{
		try { return _config_rom.xml().sub_node("vfs"); }
		catch (...) {
			Genode::error("VFS not configured");
			_env.parent().exit(~0);
			throw;
		}
	}

	Vfs::Simple_env _vfs_env { _env, _heap, vfs_config() };

	Constructible<Reporter> _devices_reporter { };
	Constructible<Reporter> _focus_reporter   { };

	typedef String<80> Version;

	void _report(Reporter &reporter, Version const &version)
	{
		Reporter::Xml_generator xml(reporter, [&] () {
			xml.attribute("version", version); });
	}

	Constructible<Attached_rom_dataspace> _devices_rom { };
	Constructible<Attached_rom_dataspace> _focus_rom   { };

	Signal_handler<Main> _devices_rom_update_handler {
		_env.ep(), *this, &Main::_handle_devices_rom_update };

	Signal_handler<Main> _focus_rom_update_handler {
		_env.ep(), *this, &Main::_handle_focus_rom_update };

	Signal_handler<Main> _focus_removal_handler {
		_env.ep(), *this, &Main::_handle_focus_removal };

	Constructible<Timer::One_shot_timeout<Main> > _one_shot_timeout { };

	void _handle_init()
	{
		log("(1) check initial content of \"devices\" ROM");
		_devices_rom.construct(_env, "devices");
		if (_devices_rom->xml().attribute_value("version", Version()) != "initial") {
			error("ROM does not contain expected initial conent");
			throw Exception();
		}

		log("(2) issue new \"devices\" report before installing a ROM signal handler");
		_devices_reporter.construct(_env, "devices");
		_devices_reporter->enabled(true);
		_report(*_devices_reporter, "version 2");

		log("(3) wait a bit to let the report reach the RAM fs");
		_one_shot_timeout.construct(_timer, *this, &Main::_handle_timer_1);
		_one_shot_timeout->schedule(Microseconds(500*1000));
	}

	void _handle_timer_1(Duration)
	{
		log("(4) install ROM signal handler, is expected to trigger immediately");

		_devices_rom->sigh(_devices_rom_update_handler);
	}

	void _handle_devices_rom_update()
	{
		log("(5) received ROM update as expected");

		_devices_rom->update();
		if (_devices_rom->xml().attribute_value("version", Version()) != "version 2") {
			error("(6) unexpected content of \"devices\" ROM after update");
			throw Exception();
		}

		log("(6) request not-yet-available \"focus\" ROM");

		_focus_rom.construct(_env, "focus");
		_focus_rom->sigh(_focus_rom_update_handler);

		log("(7) wait a bit until generating the focus report");
		_one_shot_timeout.construct(_timer, *this, &Main::_handle_timer_2);
		_one_shot_timeout->schedule(Microseconds(500*1000));
	}

	void _handle_timer_2(Duration)
	{
		log("(8) generate \"focus\" report, is expected to trigger ROM notification");
		_focus_reporter.construct(_env, "focus");
		_focus_reporter->enabled(true);
		_report(*_focus_reporter, "focus version 1");
	}

	void _handle_focus_rom_update()
	{
		_focus_rom->update();

		if (_focus_rom->xml().attribute_value("version", Version()) != "focus version 1") {
			error("(9) unexpected content of \"focus\" ROM");
			throw Exception();
		}

		log("(9) received expected focus ROM content");

		_focus_rom->sigh(_focus_removal_handler);
		_one_shot_timeout.construct(_timer, *this, &Main::_handle_timer_3);
		_one_shot_timeout->schedule(Microseconds(500*1000));
	}

	void _handle_timer_3(Duration)
	{
		log("(10) remove focus file");
		_vfs_env.root_dir().unlink("focus");
	}

	void _handle_focus_removal()
	{
		_focus_rom->update();

		if (!_focus_rom->xml().has_type("empty")) {
			error("(11) unexpected content of \"focus\" ROM");
			throw Exception();
		}

		log("(11) received empty focus ROM");

		/* test completed successfully */
		_env.parent().exit(0);
	}

	Main(Env &env) : _env(env)
	{
		log("--- test-fs_report started ---");

		_handle_init();
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

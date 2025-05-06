/*
 * \brief  Test for report-ROM service
 * \author Norman Feske
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <timer_session/connection.h>


#define ASSERT(cond) \
	if (!(cond)) { \
		error("assertion ", #cond, " failed"); \
		throw -2; }


namespace Test {
	struct Main;
	using namespace Genode;
}


struct Test::Main
{
	Env &_env;

	Timer::Connection _timer { _env };

	Constructible<Expanding_reporter> _brightness_reporter { };

	void _report_brightness(int value)
	{
		_brightness_reporter->generate([&] (Xml_generator &xml) {
			xml.attribute("value", value); });
	}

	Constructible<Attached_rom_dataspace> _brightness_rom { };

	enum State { WAIT_FOR_FIRST_UPDATE,
	             WAIT_FOR_TIMEOUT,
	             WAIT_FOR_SECOND_UPDATE } _state = WAIT_FOR_FIRST_UPDATE;

	void _handle_rom_update()
	{
		if (_state == WAIT_FOR_FIRST_UPDATE) {

			log("ROM client: got signal");

			log("ROM client: request updated brightness report");
			_brightness_rom->update();
			log("         -> ", _brightness_rom->local_addr<char const>());

			log("Reporter: close report session, wait a bit");
			_brightness_reporter.destruct();

			_timer.trigger_once(250*1000);
			_state = WAIT_FOR_TIMEOUT;
			return;
		}

		if (_state == WAIT_FOR_SECOND_UPDATE) {
			try {
				log("ROM client: try to open the same report again");
				Expanding_reporter again { _env, "brightness" };
				error("expected Service_denied");
				throw -3;
			}
			catch (Service_denied) {
				log("ROM client: caught Service_denied - OK"); }

			log("--- test-report_rom finished ---");
			_env.parent().exit(0);
			return;
		}
	}

	Signal_handler<Main> _rom_update_handler {
		_env.ep(), *this, &Main::_handle_rom_update };

	void _handle_timer()
	{
		if (_state == WAIT_FOR_TIMEOUT) {
			log("got timeout");
			String<100> rom_content(_brightness_rom->local_addr<char const>()),
			            expected("<brightness value=\"77\"/>\n");
			log("         -> ", rom_content);
			if (rom_content != expected) {
				error("unexpected ROM content: '", rom_content, "'");
				_env.parent().exit(-1);
			}
			_brightness_rom->update();
			ASSERT(_brightness_rom->valid());
			log("ROM client: ROM is available despite report was closed - OK");

			log("Reporter: start reporting (while the ROM client still listens)");
			_brightness_reporter.construct(_env, "brightness");
			_report_brightness(99);

			log("ROM client: wait for update notification");
			_state = WAIT_FOR_SECOND_UPDATE;
			return;
		}
	}

	Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	Main(Env &env) : _env(env)
	{
		log("--- test-report_rom started ---");

		_timer.sigh(_timer_handler);

		log("Reporter: open session");
		_brightness_reporter.construct(_env, "brightness");

		log("Reporter: brightness 10");
		_report_brightness(10);

		log("ROM client: request brightness report");
		_brightness_rom.construct(_env, "brightness");

		ASSERT(_brightness_rom->valid());

		_brightness_rom->sigh(_rom_update_handler);
		log("         -> ", _brightness_rom->local_addr<char const>());

		log("Reporter: updated brightness to 77");
		_report_brightness(77);

		log("ROM client: wait for update notification");
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

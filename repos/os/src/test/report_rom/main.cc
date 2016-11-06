/*
 * \brief  Test for report-ROM service
 * \author Norman Feske
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/component.h>
#include <os/reporter.h>
#include <os/attached_rom_dataspace.h>
#include <timer_session/connection.h>


#define ASSERT(cond) \
	if (!(cond)) { \
		Genode::error("assertion ", #cond, " failed"); \
		throw -2; }


static void report_brightness(Genode::Reporter &reporter, int value)
{
	Genode::Reporter::Xml_generator xml(reporter, [&] () {
		xml.attribute("brightness", value); });
}


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	Signal_receiver sig_rec;
	Signal_context  sig_ctx;
	Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

	log("--- test-report_rom started ---");

	log("Reporter: open session");
	Reporter brightness_reporter("brightness");
	brightness_reporter.enabled(true);

	log("Reporter: brightness 10");
	report_brightness(brightness_reporter, 10);

	log("ROM client: request brightness report");
	Attached_rom_dataspace brightness_rom("brightness");

	ASSERT(brightness_rom.valid());

	brightness_rom.sigh(sig_cap);
	log("         -> ", brightness_rom.local_addr<char const>());

	log("Reporter: updated brightness to 77");
	report_brightness(brightness_reporter, 77);

	log("ROM client: wait for update notification");
	sig_rec.wait_for_signal();
	log("ROM client: got signal");

	log("ROM client: request updated brightness report");
	brightness_rom.update();
	log("         -> ", brightness_rom.local_addr<char const>());

	log("Reporter: close report session");
	brightness_reporter.enabled(false);

	/* give report_rom some time to close the report session */
	Timer::Connection timer;
	timer.msleep(250);

	brightness_rom.update();
	ASSERT(brightness_rom.valid());
	log("ROM client: ROM is available despite report was closed - OK");

	log("Reporter: start reporting (while the ROM client still listens)");
	brightness_reporter.enabled(true);
	report_brightness(brightness_reporter, 99);

	log("ROM client: wait for update notification");
	sig_rec.wait_for_signal();

	try {
		log("ROM client: try to open the same report again");
		Reporter again("brightness");
		again.enabled(true);
		error("expected Service_denied");
		throw -3;
	} catch (Genode::Parent::Service_denied) {
		log("ROM client: catched Parent::Service_denied - OK");
	}

	log("--- test-report_rom finished ---");

	sig_rec.dissolve(&sig_ctx);

	env.parent().exit(0);
}

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

#include <base/printf.h>
#include <os/reporter.h>
#include <os/attached_rom_dataspace.h>
#include <timer_session/connection.h>


#define ASSERT(cond) \
	if (!(cond)) { \
		PERR("assertion %s failed", #cond); \
		return -2; }


static void report_brightness(Genode::Reporter &reporter, int value)
{
	Genode::Reporter::Xml_generator xml(reporter, [&] () {
		xml.attribute("brightness", value); });
}


int main(int argc, char **argv)
{
	using namespace Genode;

	Signal_receiver sig_rec;
	Signal_context  sig_ctx;
	Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

	printf("--- test-report_rom started ---\n");

	printf("Reporter: open session\n");
	Reporter brightness_reporter("brightness");
	brightness_reporter.enabled(true);

	printf("Reporter: brightness 10\n");
	report_brightness(brightness_reporter, 10);

	printf("ROM client: request brightness report\n");
	Attached_rom_dataspace brightness_rom("brightness");

	ASSERT(brightness_rom.is_valid());

	brightness_rom.sigh(sig_cap);
	printf("         -> %s\n", brightness_rom.local_addr<char>());

	printf("Reporter: updated brightness to 77\n");
	report_brightness(brightness_reporter, 77);

	printf("ROM client: wait for update notification\n");
	sig_rec.wait_for_signal();
	printf("ROM client: got signal\n");

	printf("ROM client: request updated brightness report\n");
	brightness_rom.update();
	printf("         -> %s\n", brightness_rom.local_addr<char>());

	printf("Reporter: close report session\n");
	brightness_reporter.enabled(false);

	/* give report_rom some time to close the report session */
	static Timer::Connection timer;
	timer.msleep(250);

	brightness_rom.update();
	ASSERT(brightness_rom.is_valid());
	printf("ROM client: ROM is available despite report was closed - OK\n");

	printf("Reporter: start reporting (while the ROM client still listens)\n");
	brightness_reporter.enabled(true);
	report_brightness(brightness_reporter, 99);

	printf("ROM client: wait for update notification\n");
	sig_rec.wait_for_signal();

	try {
		printf("ROM client: try to open the same report again\n");
		Reporter again("brightness");
		again.enabled(true);
		PERR("expected Service_denied");
		return -3;
	} catch (Genode::Parent::Service_denied) {
		printf("ROM client: catched Parent::Service_denied - OK\n");
	}

	printf("--- test-report_rom finished ---\n");

	sig_rec.dissolve(&sig_ctx);

	return 0;
}

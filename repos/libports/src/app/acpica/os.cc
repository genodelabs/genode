/*
 * \brief  Example app to utilize ACPICA library
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/log.h>
#include <base/signal.h>
#include <irq_session/connection.h>
#include <io_port_session/connection.h>

#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/reporter.h>

#include <util/volatile_object.h>
#include <util/xml_node.h>

extern "C" {
#include "acpi.h"
#include "accommon.h"
#include "acevents.h"
#include "acnamesp.h"
}

namespace Acpica {
	struct Main;
	struct Statechange;
	class Reportstate;
};

#include "util.h"
#include "reporter.h"
#include "fixed.h"
#include "ac.h"
#include "lid.h"
#include "sb.h"
#include "ec.h"


static void init_acpica(Acpica::Reportstate *report) {

	/* enable debugging: */
	/* AcpiDbgLevel |= ACPI_LV_IO | ACPI_LV_INTERRUPTS | ACPI_LV_INIT_NAMES; */

	ACPI_STATUS status = AcpiInitializeSubsystem();
	if (status != AE_OK) {
		Genode::error("AcpiInitializeSubsystem failed, status=", status);
		return;
	}

	status = AcpiInitializeTables(nullptr, 0, true);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeTables failed, status=", status);
		return;
	}

	status = AcpiLoadTables();
	if (status != AE_OK) {
		Genode::error("AcpiLoadTables failed, status=", status);
		return;
	}

	status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (status != AE_OK) {
		Genode::error("AcpiEnableSubsystem failed, status=", status);
		return;
	}

	status = AcpiInitializeObjects(ACPI_NO_DEVICE_INIT);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeObjects (no devices) failed, status=", status);
		return;
	}

	/* Embedded controller */
	status = AcpiGetDevices(ACPI_STRING("PNP0C09"), Ec::detect, report, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices failed, status=", status);
		return;
	}

	status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (status != AE_OK) {
		Genode::error("AcpiInitializeObjects (full init) failed, status=", status);
		return;
	}

	status = AcpiUpdateAllGpes();
	if (status != AE_OK) {
		Genode::error("AcpiUpdateAllGpes failed, status=", status);
		return;
	}

	status = AcpiEnableAllRuntimeGpes();
	if (status != AE_OK) {
		Genode::error("AcpiEnableAllRuntimeGpes failed, status=", status);
		return;
	}

	/* note: ACPI_EVENT_PMTIMER claimed by nova kernel - not usable by us */
	Fixed * acpi_fixed = new (Genode::env()->heap()) Fixed(report);

	status = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
	                                      Fixed::handle_power_button,
	                                      acpi_fixed);
	if (status != AE_OK)
		Genode::log("failed   - power button registration - error=", status);

	status = AcpiInstallFixedEventHandler(ACPI_EVENT_SLEEP_BUTTON,
	                                      Fixed::handle_sleep_button,
	                                      acpi_fixed);
	if (status != AE_OK)
		Genode::log("failed   - sleep button registration - error=", status);


	/* AC Adapters and Power Source Objects */
	status = AcpiGetDevices(ACPI_STRING("ACPI0003"), Ac::detect, report, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (ACPI0003) failed, status=", status);
		return;
	}

	/* Smart battery control devices */
	status = AcpiGetDevices(ACPI_STRING("PNP0C0A"), Battery::detect, report, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (PNP0C0A) failed, status=", status);
		return;
	}

	/* LID device */
	status = AcpiGetDevices(ACPI_STRING("PNP0C0D"), Lid::detect, report, nullptr);
	if (status != AE_OK) {
		Genode::error("AcpiGetDevices (PNP0C0D) failed, status=", status);
		return;
	}
}

struct Acpica::Statechange
{
	Genode::Signal_handler<Acpica::Statechange> _dispatcher;
	Genode::Attached_rom_dataspace _system_state;
	bool _enable_reset;
	bool _enable_poweroff;

	Statechange(Genode::Entrypoint &ep, bool reset, bool poweroff)
	:
		_dispatcher(ep, *this, &Statechange::state_changed),
		_system_state("system"),
		_enable_reset(reset), _enable_poweroff(poweroff)
	{
		_system_state.sigh(_dispatcher);

		state_changed();
	}

	void state_changed() {

		_system_state.update();

		if (!_system_state.is_valid()) return;

		Genode::Xml_node system(_system_state.local_addr<char>(),
		                       _system_state.size());

		Genode::String<32> state;
		system.attribute("state").value<32>(&state);

		if (_enable_poweroff && state == "poweroff") {
			ACPI_STATUS res0 = AcpiEnterSleepStatePrep(5);
			ACPI_STATUS res1 = AcpiEnterSleepState(5);
			Genode::error("system poweroff failed - "
			              "res=", Genode::Hex(res0), ",", Genode::Hex(res1));
			return;
		}

		if (_enable_reset && state == "reset") {
			ACPI_STATUS res = AE_OK;
			try {
				res = AcpiReset();
			} catch (...) { }

			Genode::uint64_t const space_addr = AcpiGbl_FADT.ResetRegister.Address;
			Genode::error("system reset failed - "
			              "err=", res, " "
			              "reset=", !!(AcpiGbl_FADT.Flags & ACPI_FADT_RESET_REGISTER), " "
			              "spaceid=", Genode::Hex(AcpiGbl_FADT.ResetRegister.SpaceId), " "
			              "addr=", Genode::Hex(space_addr));
		}
	}
};

struct Acpica::Main {

	Genode::Signal_handler<Acpica::Main> _sci_irq;
	Genode::Lazy_volatile_object<Genode::Irq_connection> _sci_conn;

	Acpica::Reportstate * _report = nullptr;

	static struct Irq_handler {
		UINT32 irq;
		ACPI_OSD_HANDLER handler;
		void *context;
	} irq_handler;

	Main(Genode::Env &env)
	:
		_sci_irq(env.ep(), *this, &Main::acpi_irq)
	{
		bool enable_reset = Genode::config()->xml_node().attribute_value("reset", false);
		bool enable_poweroff = Genode::config()->xml_node().attribute_value("poweroff", false);
		bool enable_report = Genode::config()->xml_node().attribute_value("report", false);
		bool enable_ready = Genode::config()->xml_node().attribute_value("acpi_ready", false);

		if (enable_report)
			_report = new (Genode::env()->heap()) Acpica::Reportstate();

		init_acpica(_report);

		if (enable_report)
			_report->enable();

		if (enable_reset || enable_poweroff)
			new (Genode::env()->heap()) Acpica::Statechange(env.ep(), enable_reset,
			                                                enable_poweroff);

		/* setup IRQ */
		if (!irq_handler.handler) {
			Genode::warning("no IRQ handling available");
			return;
		}

		_sci_conn.construct(irq_handler.irq);

		Genode::log("SCI IRQ: ", irq_handler.irq);

		_sci_conn->sigh(_sci_irq);
		_sci_conn->ack_irq();

		if (!enable_ready)
			return;

		/* we are ready - signal it via changing system state */
		const char * system_file = "system";

		static Genode::Reporter _system_rom { "system", "acpi_ready" };
		_system_rom.enabled(true);
		Genode::Reporter::Xml_generator xml(_system_rom, [&] () {
			xml.attribute("state", "acpi_ready");
		});
	}

	void acpi_irq()
	{
		if (!irq_handler.handler)
			return;

		UINT32 res = irq_handler.handler(irq_handler.context);

		_sci_conn->ack_irq();

		AcpiOsWaitEventsComplete();

		if (_report)
			_report->generate_report();

		if (res == ACPI_INTERRUPT_HANDLED)
			return;
	}
};

struct Acpica::Main::Irq_handler Acpica::Main::irq_handler;


ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler,
                                          void *context)
{
	Acpica::Main::irq_handler.irq = irq;
	Acpica::Main::irq_handler.handler = handler;
	Acpica::Main::irq_handler.context = context;
	return AE_OK;
}


Genode::size_t Component::stack_size()      { return 4*1024*sizeof(Genode::addr_t); }
void Component::construct(Genode::Env &env) { static Acpica::Main main(env); }

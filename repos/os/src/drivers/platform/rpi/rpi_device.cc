/*
 * \brief  Platform driver - Device abstraction for rpi
 * \author Stefan Kalkowski
 * \date   2020-08-17
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <property_command.h>
#include <property_message.h>
#include <rpi_device.h>
#include <session_component.h>


unsigned Driver::Rpi_device::Power_domain::id()
{
	if (name == "sdhci")   { return 0; }
	if (name == "uart_0")  { return 1; }
	if (name == "uart_1")  { return 2; }
	if (name == "usb")     { return 3; }
	if (name == "i2c_0")   { return 4; }
	if (name == "i2c_1")   { return 5; }
	if (name == "i2c_2")   { return 6; }
	if (name == "spi")     { return 7; }
	if (name == "ccp2tx")  { return 8; }

	warning("Invalid power-domain ", name);
	return ~0U;
};


void Driver::Rpi_device::acquire(Driver::Session_component & sc)
{
	Driver::Device::acquire(sc);

	_power_domain_list.for_each([&] (Power_domain & p) {
		auto & msg = sc.env().mbox.message<Property_message>();
		msg.append_no_response<Property_command::Set_power_state>(p.id(),
		                                                          true,
		                                                          true);
		sc.env().mbox.call<Property_message>();
	});
}


void Driver::Rpi_device::release(Session_component & sc)
{
	_power_domain_list.for_each([&] (Power_domain & p) {
		auto & msg = sc.env().mbox.message<Property_message>();
		msg.append_no_response<Property_command::Set_power_state>(p.id(),
		                                                          true,
		                                                          true);
		sc.env().mbox.call<Property_message>();
	});

	return Driver::Device::release(sc);
}


void Driver::Rpi_device::_report_platform_specifics(Genode::Xml_generator     &,
                                                    Driver::Session_component &)
{
	/*
	 * Normally, the platform driver should report about clock settings of the
	 * device etc. here. But we do not implement clocking for RPI yet.
	 */
}

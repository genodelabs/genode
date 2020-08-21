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


bool Driver::Rpi_device::acquire(Driver::Session_component & sc)
{
	bool ret = Driver::Device::acquire(sc);

	if (ret) {
		_power_domain_list.for_each([&] (Power_domain & p) {
			auto & msg = sc.env().mbox.message<Property_message>();
			msg.append_no_response<Property_command::Set_power_state>(p.id(),
			                                                          true,
			                                                          true);
			sc.env().mbox.call<Property_message>();
		});
	}

	return ret;
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


void Driver::Rpi_device::_report_platform_specifics(Genode::Xml_generator     & /*xml*/,
                                                    Driver::Session_component & /*sc*/)
{
	//_clock_list.for_each([&] (Clock & c) {
	//	Avl_string_base * asb =
	//		sc.env().ccm.tree.first()->find_by_name(c.name.string());
	//	if (!asb || !c.driver_name.valid()) { return; }
	//	Driver::Clock & clock =
	//		static_cast<Driver::Clock::Clock_tree_element*>(asb)->object();
	//	xml.node("clock", [&] () {
	//		xml.attribute("rate", clock.get_rate());
	//		xml.attribute("name", c.driver_name);
	//	});
	//});
}

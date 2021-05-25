/*
 * \brief  Platform driver - Device abstraction for i.MX
 * \author Stefan Kalkowski
 * \date   2020-08-17
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <imx_device.h>
#include <clock.h>
#include <session_component.h>


bool Driver::Imx_device::acquire(Driver::Session_component & sc)
{
	bool ret = Driver::Device::acquire(sc);

	if (ret) {
		_power_domain_list.for_each([&] (Power_domain & p) {
			sc.env().gpc.enable(p.name); });
		_reset_domain_list.for_each([&] (Reset_domain & r) {
			sc.env().src.enable(r.name); });
		_clock_list.for_each([&] (Clock & c) {
			Avl_string_base * asb =
				sc.env().ccm.tree.first()->find_by_name(c.name.string());
			if (!asb) {
				Genode::warning("Clock ", c.name, " is unknown! ");
				return;
			}
			Driver::Clock & clock =
				static_cast<Driver::Clock::Clock_tree_element*>(asb)->object();
			if (c.parent.valid()) { clock.set_parent(c.parent); }
			if (c.rate)           { clock.set_rate(c.rate);     }
			clock.enable();
		});

		sc.update_devices_rom();
	}

	return ret;
}


void Driver::Imx_device::release(Session_component & sc)
{
	_reset_domain_list.for_each([&] (Reset_domain & r) {
		sc.env().src.disable(r.name); });
	_power_domain_list.for_each([&] (Power_domain & p) {
		sc.env().gpc.disable(p.name); });
	_clock_list.for_each([&] (Clock & c) {
		Avl_string_base * asb =
			sc.env().ccm.tree.first()->find_by_name(c.name.string());
		if (!asb) { return; }
		static_cast<Driver::Clock::Clock_tree_element*>(asb)->object().disable();
	});

	return Driver::Device::release(sc);
}


void Driver::Imx_device::_report_platform_specifics(Genode::Xml_generator     & xml,
                                                    Driver::Session_component & sc)
{
	_clock_list.for_each([&] (Clock & c) {
		Avl_string_base * asb =
			sc.env().ccm.tree.first()->find_by_name(c.name.string());
		if (!asb || !c.driver_name.valid()) { return; }
		Driver::Clock & clock =
			static_cast<Driver::Clock::Clock_tree_element*>(asb)->object();
		xml.node("clock", [&] () {
			xml.attribute("rate", clock.get_rate());
			xml.attribute("name", c.driver_name);
		});
	});
}

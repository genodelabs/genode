/*
 * \brief  Platform driver - Device abstraction
 * \author Stefan Kalkowski
 * \date   2020-04-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <device.h>
#include <pci.h>
#include <device_component.h>
#include <session_component.h>


Driver::Device::Owner::Owner(Session_component & session)
: obj_id(reinterpret_cast<void*>(&session)) {}


Driver::Device::Name Driver::Device::name() const { return _name; }


Driver::Device::Type Driver::Device::type() const { return _type; }


Driver::Device::Owner Driver::Device::owner() const { return _owner; }


void Driver::Device::acquire(Session_component & sc)
{
	if (!_owner.valid()) _owner = sc;

	_power_domain_list.for_each([&] (Power_domain & p) {

		bool ok = false;
		sc.devices().powers().apply(p.name, [&] (Driver::Power &power) {
			power.on();
			ok = true;
		});

		if (!ok)
			warning("power domain ", p.name, " is unknown");
	});

	_reset_domain_list.for_each([&] (Reset_domain & r) {

		bool ok = false;
		sc.devices().resets().apply(r.name, [&] (Driver::Reset &reset) {
			reset.deassert();
			ok = true;
		});

		if (!ok)
			warning("reset domain ", r.name, " is unknown");
	});

	_clock_list.for_each([&] (Clock &c) {

		bool ok = false;
		sc.devices().clocks().apply(c.name, [&] (Driver::Clock &clock) {

			if (c.parent.valid())
				clock.parent(c.parent);

			if (c.rate)
				clock.rate(Driver::Clock::Rate { c.rate });

			clock.enable();
			ok = true;
		});

		if (!ok) {
			warning("clock ", c.name, " is unknown");
			return;
		}
	});

	pci_enable(sc.env(), sc.device_pd(), *this);
	sc.update_devices_rom();
	sc.devices().update_report();
}


void Driver::Device::release(Session_component & sc)
{
	if (!(_owner == sc))
		return;

	pci_disable(sc.env(), *this);

	_reset_domain_list.for_each([&] (Reset_domain & r)
	{
		sc.devices().resets().apply(r.name, [&] (Driver::Reset &reset) {
			reset.assert(); });
	});

	_power_domain_list.for_each([&] (Power_domain & p)
	{
		sc.devices().powers().apply(p.name, [&] (Driver::Power &power) {
			power.off(); });
	});

	_clock_list.for_each([&] (Clock & c)
	{
		sc.devices().clocks().apply(c.name, [&] (Driver::Clock &clock) {
			clock.disable(); });
	});

	_owner = Owner();
	sc.update_devices_rom();
	sc.devices().update_report();
}


void Driver::Device::report(Xml_generator & xml, Device_model & devices,
                            bool info)
{
	xml.node("device", [&] () {
		xml.attribute("name", name());
		xml.attribute("type", type());
		xml.attribute("used", _owner.valid());
		_io_mem_list.for_each([&] (Io_mem & io_mem) {
			xml.node("io_mem", [&] () {
				if (!info)
					return;
				xml.attribute("phys_addr", String<16>(Hex(io_mem.range.start)));
				xml.attribute("size",      String<16>(Hex(io_mem.range.size)));
			});
		});
		_irq_list.for_each([&] (Irq & irq) {
			xml.node("irq", [&] () {
				if (!info)
					return;
				xml.attribute("number", irq.number);
			});
		});
		_io_port_range_list.for_each([&] (Io_port_range & io_port_range) {
			xml.node("io_port_range", [&] () {
				if (!info)
					return;
				xml.attribute("phys_addr", String<16>(Hex(io_port_range.addr)));
				xml.attribute("size",      String<16>(Hex(io_port_range.size)));
			});
		});
		_property_list.for_each([&] (Property & p) {
			xml.node("property", [&] () {
				xml.attribute("name",  p.name);
				xml.attribute("value", p.value);
			});
		});
		_clock_list.for_each([&] (Clock &c) {
			devices.clocks().apply(c.name, [&] (Driver::Clock &clock) {
				xml.node("clock", [&] () {
					xml.attribute("rate", clock.rate().value);
					xml.attribute("name", c.driver_name);
				});
			});
		});
		_pci_config_list.for_each([&] (Pci_config &pci) {
			xml.node("pci-config", [&] () {
				xml.attribute("vendor_id", String<16>(Hex(pci.vendor_id)));
				xml.attribute("device_id", String<16>(Hex(pci.device_id)));
				xml.attribute("class",     String<16>(Hex(pci.class_code)));
			});
		});

		_report_platform_specifics(xml, devices);
	});
}


Driver::Device::Device(Name name, Type type)
: _name(name), _type(type) { }


Driver::Device::~Device()
{
	if (_owner.valid()) {
		error("Device to be destroyed, still obtained by session"); }
}


void Driver::Device_model::update_report()
{
	if (_reporter.enabled()) {
		Reporter::Xml_generator xml(_reporter, [&] () {
			for_each([&] (Device & device) {
				device.report(xml, *this, true); });
		});
	}
}


void Driver::Device_model::update(Xml_node const & node)
{
	_model.update_from_xml(*this, node);
	update_report();
}

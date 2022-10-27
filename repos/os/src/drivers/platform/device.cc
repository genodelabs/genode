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

#include <util/bit_array.h>

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
		_model.powers().apply(p.name, [&] (Driver::Power &power) {
			power.on();
			ok = true;
		});

		if (!ok)
			warning("power domain ", p.name, " is unknown");
	});

	_reset_domain_list.for_each([&] (Reset_domain & r) {

		bool ok = false;
		_model.resets().apply(r.name, [&] (Driver::Reset &reset) {
			reset.deassert();
			ok = true;
		});

		if (!ok)
			warning("reset domain ", r.name, " is unknown");
	});

	_clock_list.for_each([&] (Clock &c) {

		bool ok = false;
		_model.clocks().apply(c.name, [&] (Driver::Clock &clock) {

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

	pci_enable(_env, sc.device_pd(), *this);
	sc.update_devices_rom();
	_model.device_status_changed();
}


void Driver::Device::release(Session_component & sc)
{
	if (!(_owner == sc))
		return;

	if (!_leave_operational) {
		pci_disable(_env, *this);

		_reset_domain_list.for_each([&] (Reset_domain & r)
		{
			_model.resets().apply(r.name, [&] (Driver::Reset &reset) {
				reset.assert(); });
		});

		_power_domain_list.for_each([&] (Power_domain & p)
		{
			_model.powers().apply(p.name, [&] (Driver::Power &power) {
				power.off(); });
		});

		_clock_list.for_each([&] (Clock & c)
		{
			_model.clocks().apply(c.name, [&] (Driver::Clock &clock) {
				clock.disable(); });
		});
	}

	_owner = Owner();
	sc.update_devices_rom();
	_model.device_status_changed();
}


void Driver::Device::generate(Xml_generator & xml, bool info) const
{
	xml.node("device", [&] () {
		xml.attribute("name", name());
		xml.attribute("type", type());
		xml.attribute("used", _owner.valid());
		_io_mem_list.for_each([&] (Io_mem const & io_mem) {
			xml.node("io_mem", [&] () {
				if (io_mem.bar.valid())
					xml.attribute("pci_bar", io_mem.bar.number);
				if (!info)
					return;
				xml.attribute("phys_addr", String<16>(Hex(io_mem.range.start)));
				xml.attribute("size",      String<16>(Hex(io_mem.range.size)));
			});
		});
		_irq_list.for_each([&] (Irq const & irq) {
			xml.node("irq", [&] () {
				if (!info)
					return;
				xml.attribute("number", irq.number);
				if (irq.shared) xml.attribute("shared", true);
			});
		});
		_io_port_range_list.for_each([&] (Io_port_range const & iop) {
			xml.node("io_port_range", [&] () {
				if (iop.bar.valid())
					xml.attribute("pci_bar", iop.bar.number);
				if (!info)
					return;
				xml.attribute("phys_addr", String<16>(Hex(iop.range.addr)));
				xml.attribute("size",      String<16>(Hex(iop.range.size)));
			});
		});
		_property_list.for_each([&] (Property const & p) {
			xml.node("property", [&] () {
				xml.attribute("name",  p.name);
				xml.attribute("value", p.value);
			});
		});
		_clock_list.for_each([&] (Clock const & c) {
			_model.clocks().apply(c.name, [&] (Driver::Clock &clock) {
				xml.node("clock", [&] () {
					xml.attribute("rate", clock.rate().value);
					xml.attribute("name", c.driver_name);
				});
			});
		});
		_pci_config_list.for_each([&] (Pci_config const & pci) {
			xml.node("pci-config", [&] () {
				xml.attribute("vendor_id", String<16>(Hex(pci.vendor_id)));
				xml.attribute("device_id", String<16>(Hex(pci.device_id)));
				xml.attribute("class",     String<16>(Hex(pci.class_code)));
				xml.attribute("revision",  String<16>(Hex(pci.revision)));
				xml.attribute("sub_vendor_id", String<16>(Hex(pci.sub_vendor_id)));
				xml.attribute("sub_device_id", String<16>(Hex(pci.sub_device_id)));
				pci_device_specific_info(*this, _env, _model, xml);
			});
		});
	});
}


Driver::Device::Device(Env & env, Device_model & model, Name name, Type type,
                       bool leave_operational)
:
	_env(env), _model(model), _name(name), _type(type),
	_leave_operational(leave_operational) { }


Driver::Device::~Device()
{
	if (_owner.valid()) {
		error("Device to be destroyed, still obtained by session"); }
}


void Driver::Device_model::device_status_changed()
{
	_reporter.update_report();
};


void Driver::Device_model::generate(Xml_generator & xml) const
{
	for_each([&] (Device const & device) {
		device.generate(xml, true); });
}


void Driver::Device_model::update(Xml_node const & node)
{
	_model.update_from_xml(*this, node);

	/*
	 * Detect all shared interrupts
	 */
	enum { MAX_IRQ = 1024 };
	Bit_array<MAX_IRQ> detected_irqs, shared_irqs;
	for_each([&] (Device const & device) {
		device._irq_list.for_each([&] (Device::Irq const & irq) {

			if (irq.type != Device::Irq::LEGACY)
				return;

			if (detected_irqs.get(irq.number, 1)) {
				if (!shared_irqs.get(irq.number, 1))
					shared_irqs.set(irq.number, 1);
			} else
				detected_irqs.set(irq.number, 1);
		});
	});

	/*
	 * Mark all shared interrupts in the devices
	 */
	for_each([&] (Device & device) {
		device._irq_list.for_each([&] (Device::Irq & irq) {

			if (irq.type != Device::Irq::LEGACY)
				return;

			if (shared_irqs.get(irq.number, 1))
				irq.shared = true;
		});
	});

	/*
	 * Create shared interrupt objects
	 */
	for (unsigned i = 0; i < MAX_IRQ; i++) {
		if (!shared_irqs.get(i, 1))
			continue;
		bool found = false;
		_shared_irqs.for_each([&] (Shared_interrupt & sirq) {
			if (sirq.number() == i) found = true; });
		if (!found)
			new (_heap) Shared_interrupt(_shared_irqs, _env, i);
	}

	/*
	 * Iterate over all devices and apply PCI quirks if necessary
	 */
	for_each([&] (Device const & device) {
		pci_apply_quirks(_env, device);
	});
}

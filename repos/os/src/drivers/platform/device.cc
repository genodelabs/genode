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
#include <device_owner.h>
#include <device_component.h>


Driver::Device::Owner::Owner(Device_owner & owner)
: obj_id(reinterpret_cast<void*>(&owner)) {}


Driver::Device::Name Driver::Device::name() const { return _name; }


Driver::Device::Type Driver::Device::type() const { return _type; }


Driver::Device::Owner Driver::Device::owner() const { return _owner; }


void Driver::Device::acquire(Device_owner & owner)
{
	if (!_owner.valid()) _owner = owner;

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

	owner.enable_device(*this);
	owner.update_devices_rom();

	_model.device_status_changed();
}


void Driver::Device::release(Device_owner & owner)
{
	if (!(_owner == owner))
		return;

	if (!_leave_operational) {
		owner.disable_device(*this);

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
	owner.update_devices_rom();
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


void Driver::Device::update(Allocator &alloc, Xml_node const &node)
{
	using Bar = Device::Pci_bar;

	update_list_model_from_xml(_irq_list, node,

		/* create */
		[&] (Xml_node const &node) -> Irq &
		{
			unsigned number = node.attribute_value<unsigned>("number", 0);

			Irq &irq = *(new (alloc) Irq(number));

			String<16> polarity = node.attribute_value("polarity", String<16>());
			String<16> mode     = node.attribute_value("mode",     String<16>());
			String<16> type     = node.attribute_value("type",     String<16>());
			if (polarity.valid())
				irq.polarity = (polarity == "high") ? Irq_session::POLARITY_HIGH
				                                    : Irq_session::POLARITY_LOW;
			if (mode.valid())
				irq.mode = (mode == "edge") ? Irq_session::TRIGGER_EDGE
				                            : Irq_session::TRIGGER_LEVEL;
			if (type.valid())
				irq.type = (type == "msi-x") ? Irq::MSIX : Irq::MSI;

			return irq;
		},

		/* destroy */
		[&] (Irq &irq) { destroy(alloc, &irq); },

		/* update */
		[&] (Irq &, Xml_node const &) { }
	);

	update_list_model_from_xml(_io_mem_list, node,

		/* create */
		[&] (Xml_node const &node) -> Io_mem &
		{
			using Range = Io_mem::Range;

			Bar   bar   { node.attribute_value<uint8_t>("pci_bar", Bar::INVALID) };
			Range range { node.attribute_value<addr_t>("address", 0),
			              node.attribute_value<size_t>("size",    0) };
			bool  pf    { node.attribute_value("prefetchable", false) };

			return *new (alloc) Io_mem(bar, range, pf);
		},

		/* destroy */
		[&] (Io_mem &io_mem) { destroy(alloc, &io_mem); },

		/* update */
		[&] (Io_mem &, Xml_node const &) { }
	);

	update_list_model_from_xml(_io_port_range_list, node,

		/* create */
		[&] (Xml_node const &node) -> Io_port_range &
		{
			using Range = Io_port_range::Range;

			Bar   bar   { node.attribute_value<uint8_t>("pci_bar", Bar::INVALID) };
			Range range { node.attribute_value<uint16_t>("address", 0),
			              node.attribute_value<uint16_t>("size",    0) };

			return *new (alloc) Io_port_range(bar, range);
		},

		/* destroy */
		[&] (Io_port_range &ipr) { destroy(alloc, &ipr); },

		/* update */
		[&] (Io_port_range &, Xml_node const &) { }
	);

	update_list_model_from_xml(_property_list, node,

		/* create */
		[&] (Xml_node const &node) -> Property &
		{
			return *new (alloc)
				Property(node.attribute_value("name",  Property::Name()),
				         node.attribute_value("value", Property::Value()));
		},

		/* destroy */
		[&] (Property &property) { destroy(alloc, &property); },

		/* update */
		[&] (Property &, Xml_node const &) { }
	);

	update_list_model_from_xml(_clock_list, node,

		/* create */
		[&] (Xml_node const &node) -> Clock &
		{
			return *new (alloc)
				Clock(node.attribute_value("name",        Clock::Name()),
				      node.attribute_value("parent",      Clock::Name()),
				      node.attribute_value("driver_name", Clock::Name()),
				      node.attribute_value("rate", 0UL));
		},

		/* destroy */
		[&] (Clock &clock) { destroy(alloc, &clock); },

		/* update */
		[&] (Clock &, Xml_node const &) { }
	);

	update_list_model_from_xml(_power_domain_list, node,

		/* create */
		[&] (Xml_node const &node) -> Power_domain &
		{
			return *new (alloc)
				Power_domain(node.attribute_value("name", Power_domain::Name()));
		},

		/* destroy */
		[&] (Power_domain &power) { destroy(alloc, &power); },

		/* update */
		[&] (Power_domain &, Xml_node const &) { }
	);

	update_list_model_from_xml(_reset_domain_list, node,

		/* create */
		[&] (Xml_node const &node) -> Reset_domain &
		{
			return *new (alloc)
				Reset_domain(node.attribute_value("name", Reset_domain::Name()));
		},

		/* destroy */
		[&] (Reset_domain &reset) { destroy(alloc, &reset); },

		/* update */
		[&] (Reset_domain &, Xml_node const &) { }
	);

	update_list_model_from_xml(_pci_config_list, node,

		/* create */
		[&] (Xml_node const &node) -> Pci_config &
		{
			using namespace Pci;

			addr_t   addr       = node.attribute_value("address", ~0UL);
			bus_t    bus_num    = node.attribute_value<bus_t>("bus", 0);
			dev_t    dev_num    = node.attribute_value<dev_t>("device", 0);
			func_t   func_num   = node.attribute_value<func_t>("function", 0);
			vendor_t vendor_id  = node.attribute_value<vendor_t>("vendor_id",
			                                                     0xffff);
			device_t device_id  = node.attribute_value<device_t>("device_id",
			                                                     0xffff);
			class_t  class_code = node.attribute_value<class_t>("class", 0xff);
			rev_t    rev        = node.attribute_value<rev_t>("revision", 0xff);
			vendor_t sub_v_id   = node.attribute_value<vendor_t>("sub_vendor_id",
			                                                     0xffff);
			device_t sub_d_id   = node.attribute_value<device_t>("sub_device_id",
			                                                     0xffff);
			bool     bridge     = node.attribute_value("bridge", false);

			return *(new (alloc) Pci_config(addr, bus_num, dev_num, func_num,
			                                vendor_id, device_id, class_code,
			                                rev, sub_v_id, sub_d_id, bridge));
		},

		/* destroy */
		[&] (Pci_config &pci) { destroy(alloc, &pci); },

		/* update */
		[&] (Pci_config &, Xml_node const &) { }
	);

	update_list_model_from_xml(_reserved_mem_list, node,

		/* create */
		[&] (Xml_node const &node) -> Reserved_memory &
		{
			addr_t addr = node.attribute_value("address", 0UL);
			size_t size = node.attribute_value("size",    0UL);
			return *(new (alloc) Reserved_memory({addr, size}));
		},

		/* destroy */
		[&] (Reserved_memory &reserved) { destroy(alloc, &reserved); },

		/* update */
		[&] (Reserved_memory &, Xml_node const &) { }
	);

	update_list_model_from_xml(_io_mmu_list, node,

		/* create */
		[&] (Xml_node const &node) -> Io_mmu &
		{
			return *new (alloc)
				Io_mmu(node.attribute_value("name", Io_mmu::Name()));
		},

		/* destroy */
		[&] (Io_mmu &io_mmu) { destroy(alloc, &io_mmu); },

		/* update */
		[&] (Io_mmu &, Xml_node const &) { }
	);
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
	update_list_model_from_xml(_model, node,

		/* create */
		[&] (Xml_node const &node) -> Device &
		{
			Device::Name name = node.attribute_value("name", Device::Name());
			Device::Type type = node.attribute_value("type", Device::Type());
			bool leave_operational = node.attribute_value("leave_operational", false);
			return *(new (_heap) Device(_env, *this, name, type, leave_operational));
		},

		/* destroy */
		[&] (Device &device)
		{
			device.update(_heap, Xml_node("<empty/>"));
			device.release(_owner);
			destroy(_heap, &device);
		},

		/* update */
		[&] (Device &device, Xml_node const &node)
		{
			device.update(_heap, node);
		}
	);

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

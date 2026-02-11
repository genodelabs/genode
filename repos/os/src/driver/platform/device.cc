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
#include <device_owner.h>
#include <device_component.h>
#include <kernel_io_mmu.h>
#include <pci.h>


Driver::Device::Name Driver::Device::name() const { return _name; }


Driver::Device::Type Driver::Device::type() const { return _type; }


bool Driver::Device::owner(Device_owner &dev_owner) const {
	return _owner.obj_id == (void*)&dev_owner; }


bool Driver::Device::owned() const {
	return _owner.valid(); }


void Driver::Device::acquire(Device_owner &owner)
{
	if (!_owner.valid()) _owner = owner;

	_power_domain_list.for_each([&] (Power_domain &p) {

		bool ok = false;
		_model.powers().apply(p.name, [&] (Driver::Power &power) {
			power.on();
			ok = true;
		});

		if (!ok)
			warning("power domain ", p.name, " is unknown");
	});

	_reset_domain_list.for_each([&] (Reset_domain &r) {

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

	_model.device_status_changed();
}


void Driver::Device::release(Device_owner &dev_owner)
{
	if (!owner(dev_owner))
		return;

	if (!_leave_operational) {
		dev_owner.disable_device(*this);

		_reset_domain_list.for_each([&] (Reset_domain &r)
		{
			_model.resets().apply(r.name, [&] (Driver::Reset &reset) {
				reset.assert(); });
		});

		_power_domain_list.for_each([&] (Power_domain &p)
		{
			_model.powers().apply(p.name, [&] (Driver::Power &power) {
				power.off(); });
		});

		_clock_list.for_each([&] (Clock &c)
		{
			_model.clocks().apply(c.name, [&] (Driver::Clock &clock) {
				clock.disable(); });
		});
	}

	_owner = Owner_id();
	_model.device_status_changed();
}


void Driver::Device::generate(Generator &g, bool info) const
{
	g.node("device", [&] () {
		g.attribute("name", name());
		g.attribute("type", type());
		g.attribute("used", _owner.valid());
		_io_mem_list.for_each([&] (Io_mem const &io_mem) {
			g.node("io_mem", [&] () {
				if (io_mem.bar.valid())
					g.attribute("pci_bar", io_mem.bar.number);
				if (!info)
					return;
				g.attribute("phys_addr", String<16>(Hex(io_mem.range.start)));
				g.attribute("size",      String<16>(Hex(io_mem.range.size)));
				if (io_mem.write_combined)
					g.attribute("wc", true);
			});
		});
		_irq_list.for_each([&] (Irq const &irq) {
			g.node("irq", [&] () {
				if (!info)
					return;
				g.attribute("number", irq.number);
				if (irq.shared) g.attribute("shared", true);
			});
		});
		_io_port_range_list.for_each([&] (Io_port_range const &iop) {
			g.node("io_port_range", [&] () {
				if (iop.bar.valid())
					g.attribute("pci_bar", iop.bar.number);
				if (!info)
					return;
				g.attribute("phys_addr", String<16>(Hex(iop.range.addr)));
				g.attribute("size",      String<16>(Hex(iop.range.size)));
			});
		});
		_property_list.for_each([&] (Property const &p) {
			g.node("property", [&] () {
				g.attribute("name",  p.name);
				g.attribute("value", p.value);
			});
		});
		_clock_list.for_each([&] (Clock const &c) {
			_model.clocks().apply(c.name, [&] (Driver::Clock &clock) {
				g.node("clock", [&] () {
					g.attribute("rate", clock.rate().value);
					g.attribute("name", c.driver_name);
				});
			});
		});
		_pci_config_list.for_each([&] (Pci_config const &pci) {
			g.node("pci-config", [&] () {
				g.attribute("vendor_id", String<16>(Hex(pci.vendor_id)));
				g.attribute("device_id", String<16>(Hex(pci.device_id)));
				g.attribute("class",     String<16>(Hex(pci.class_code)));
				g.attribute("revision",  String<16>(Hex(pci.revision)));
				g.attribute("sub_vendor_id", String<16>(Hex(pci.sub_vendor_id)));
				g.attribute("sub_device_id", String<16>(Hex(pci.sub_device_id)));
				pci_device_specific_info(*this, _env, _model, g);
			});
		});
	});
}


void Driver::Device::update(Allocator &alloc, Node const &node)
{
	using Bar = Device::Pci_bar;

	_irq_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Irq &
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
				irq.type = (type == "msi-x") ? Irq_session::TYPE_MSIX
				                             : Irq_session::TYPE_MSI;

			return irq;
		},

		/* destroy */
		[&] (Irq &irq) { destroy(alloc, &irq); },

		/* update */
		[&] (Irq &, Node const &) { }
	);

	_io_mem_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Io_mem &
		{
			using Range = Io_mem::Range;

			Bar   bar   { node.attribute_value<uint8_t>("pci_bar", Bar::INVALID) };
			Range range { node.attribute_value<addr_t>("address", 0),
			              node.attribute_value<size_t>("size",    0) };
			bool  wc    { node.attribute_value("wc", false) };

			return *new (alloc) Io_mem(bar, range, wc);
		},

		/* destroy */
		[&] (Io_mem &io_mem) { destroy(alloc, &io_mem); },

		/* update */
		[&] (Io_mem &, Node const &) { }
	);

	_io_port_range_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Io_port_range &
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
		[&] (Io_port_range &, Node const &) { }
	);

	_property_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Property &
		{
			return *new (alloc)
				Property(node.attribute_value("name",  Property::Name()),
				         node.attribute_value("value", Property::Value()));
		},

		/* destroy */
		[&] (Property &property) { destroy(alloc, &property); },

		/* update */
		[&] (Property &, Node const &) { }
	);

	_clock_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Clock &
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
		[&] (Clock &, Node const &) { }
	);

	_power_domain_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Power_domain &
		{
			return *new (alloc)
				Power_domain(node.attribute_value("name", Power_domain::Name()));
		},

		/* destroy */
		[&] (Power_domain &power) { destroy(alloc, &power); },

		/* update */
		[&] (Power_domain &, Node const &) { }
	);

	_reset_domain_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Reset_domain &
		{
			return *new (alloc)
				Reset_domain(node.attribute_value("name", Reset_domain::Name()));
		},

		/* destroy */
		[&] (Reset_domain &reset) { destroy(alloc, &reset); },

		/* update */
		[&] (Reset_domain &, Node const &) { }
	);

	_pci_config_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Pci_config &
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

			auto io_base_limit = node.attribute_value("io_base_limit", uint16_t(0u));
			auto memory_base   = node.attribute_value("memory_base", uint16_t(0u));
			auto memory_limit  = node.attribute_value("memory_limit", uint16_t(0u));
			auto prefetch_mb   = node.attribute_value("prefetch_memory_base", 0u);
			auto prefetch_mb_u = node.attribute_value("prefetch_memory_base_upper", 0u);
			auto prefetch_ml_u = node.attribute_value("prefetch_memory_limit_upper", 0u);

			auto io_base_limit_upper = node.attribute_value("io_base_limit_upper", 0u);
			auto expansion_rom_base  = node.attribute_value("expansion_rom_base", 0u);
			auto bridge_control      = node.attribute_value("bridge_control", uint16_t(0u));

			return *(new (alloc) Pci_config(addr, bus_num, dev_num, func_num,
			                                vendor_id, device_id, class_code,
			                                rev, sub_v_id, sub_d_id, bridge,
			                                io_base_limit, memory_base,
			                                memory_limit, prefetch_mb,
			                                prefetch_mb_u, prefetch_ml_u,
			                                io_base_limit_upper,
			                                expansion_rom_base,
			                                bridge_control));
		},

		/* destroy */
		[&] (Pci_config &pci) { destroy(alloc, &pci); },

		/* update */
		[&] (Pci_config &, Node const &) { }
	);

	_reserved_mem_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Reserved_memory &
		{
			addr_t addr = node.attribute_value("address", 0UL);
			size_t size = node.attribute_value("size",    0UL);
			return *(new (alloc) Reserved_memory({addr, size}));
		},

		/* destroy */
		[&] (Reserved_memory &reserved)
		{
			destroy(alloc, &reserved);
		},

		/* update */
		[&] (Reserved_memory &, Node const &) { }
	);

	_io_mmu_list.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Io_mmu &
		{
			return *new (alloc)
				Io_mmu(node.attribute_value("name", Io_mmu::Name()));
		},

		/* destroy */
		[&] (Io_mmu &io_mmu) { destroy(alloc, &io_mmu); },

		/* update */
		[&] (Io_mmu &, Node const &) { }
	);
}


Driver::Device::Device(Env &env, Device_model &model, Name name, Type type,
                       bool leave_operational)
:
	_env(env), _model(model), _name(name), _type(type),
	_leave_operational(leave_operational) { }


Driver::Device::~Device()
{
	if (_owner.valid()) {
		error("Device to be destroyed, still obtained by session"); }
}


void Driver::Device_model::_acquire_io_mmus()
{
	/*
	 * If the decision was made to delegate IOMMU control to the kernel
	 * keep that decision
	 */
	if (_kernel_io_mmu.constructed())
		return;

	auto init_default_mappings = [&] (auto &io_mmu) {
		for_each([&] (auto const &device) {
			device.with_io_mmu(io_mmu.name(), [&] (auto const &) {
				bool has_reserved_mem = false;
				device.for_each_reserved_memory([&] (unsigned,
				                                     Io_mmu::Range range) {
					io_mmu.add_default_range(range, range.start);
					has_reserved_mem = true;
				});

				if (!has_reserved_mem)
					return;

				/* enable default mappings for corresponding pci devices */
				device.for_pci_config([&] (Device::Pci_config const &cfg) {
					io_mmu.enable_default_mappings(
						{cfg.bus_num, cfg.dev_num, cfg.func_num});
				});
			});
		});
	};

	_io_mmu_factories.for_each([&] (auto &factory) {

		for_each([&] (auto &dev) {
			if (dev.owned())
				return;

			if (factory.matches(dev)) {
				dev.acquire(*this);
				factory.create(_heap, _io_mmus, _irq_controllers, dev);

				_io_mmus.for_each([&] (auto &io_mmu_dev) {
					if (io_mmu_dev.name() == dev.name())
						init_default_mappings(io_mmu_dev);
				});
			}
		});

	});

	bool io_mmu_avail = false;
	_io_mmus.for_each([&] (auto &io_mmu) {
		io_mmu.default_mappings_complete();
		io_mmu_avail = true;
	});

	/*
	 * If there is was no IOMMU detected, it doesn't mean there is no at all,
	 * potentially we just do not support it, but the kernel might do so
	 * (e.g. Nova on AMD platform), in that case use the kernel interface if
	 * available.
	 */
	if (!io_mmu_avail)
		_kernel_io_mmu.conditional(_kernel_controls_io_mmu, _env,
		                           _io_mmus, Device::Name { "kernel_io_mmu" });
}


void Driver::Device_model::_acquire_irq_controller()
{
	_irq_controller_factories.for_each([&] (auto &factory) {

		for_each([&] (auto &dev) {
			if (dev.owned())
				return;

			if (factory.matches(dev)) {
				dev.acquire(*this);
				factory.create(_heap, _irq_controllers, dev);
			}
		});

	});
}


void Driver::Device_model::_detect_shared_interrupts()
{
	/*
	 * Detect all shared interrupts
	 */
	enum { MAX_IRQ = 1024 };
	Bit_array<MAX_IRQ> detected_irqs, shared_irqs;

	auto shared_irq = [&] (unsigned n)
	{
		return shared_irqs.get(n, 1).convert<bool>(
			[&] (bool shared) { return shared; },
			[&] (auto &)      { return false; });
	};

	for_each([&] (auto const &device) {
		device._irq_list.for_each([&] (auto const &irq) {

			if (irq.type != Irq_session::TYPE_LEGACY)
				return;

			detected_irqs.get(irq.number, 1).with_result([&] (bool detected) {
				if (detected) {
					if (!shared_irq(irq.number))
						(void)shared_irqs.set(irq.number, 1);
				} else {
					(void)detected_irqs.set(irq.number, 1);
				}
			}, [&] (auto &) { });
		});
	});

	/*
	 * Mark all shared interrupts in the devices
	 */
	for_each([&] (auto &device) {
		device._irq_list.for_each([&] (auto &irq) {
			if (irq.type == Irq_session::TYPE_LEGACY &&
			    shared_irq(irq.number)) irq.shared = true;
		});
	});

	/*
	 * Create shared interrupt objects
	 */
	for (unsigned i = 0; i < MAX_IRQ; i++) {
		if (!shared_irq(i))
			continue;
		bool found = false;
		_shared_irqs.for_each([&] (auto &sirq) {
			if (sirq.number() == i) found = true; });
		if (!found) new (_heap) Shared_interrupt(_shared_irqs, _env, i);
	}
}


void Driver::Device_model::device_status_changed()
{
	_reporter.update_report();
};


void Driver::Device_model::report_devices(Generator &g) const
{
	for_each([&] (Device const &device) {
		device.generate(g, true); });
}


void Driver::Device_model::report_iommus(Generator &g) const
{
	for_each_io_mmu([&] (Io_mmu const &io_mmu) {
		io_mmu.generate(g); });
}


void Driver::Device_model::update(Node const &node)
{
	_model.update_from_node(node,

		/* create */
		[&] (Node const &node) -> Device &
		{
			Device::Name name = node.attribute_value("name", Device::Name());
			Device::Type type = node.attribute_value("type", Device::Type());
			bool leave_operational = node.attribute_value("leave_operational", false);
			return *(new (_heap) Device(_env, *this, name, type, leave_operational));
		},

		/* destroy */
		[&] (Device &device)
		{
			device.update(_heap, Node());
			device.release(*this);
			destroy(_heap, &device);
		},

		/* update */
		[&] (Device &device, Node const &node)
		{
			device.update(_heap, node);
		}
	);
}


void Driver::Device_model::finalize_devices_preparation()
{
	_detect_shared_interrupts();

	/*
	 * Iterate over all devices and apply PCI quirks if necessary
	 */
	for_each([&] (Device const &device) {
		pci_apply_quirks(_env, device);
	});

	_acquire_irq_controller();
	_acquire_io_mmus();
}

void Driver::Device_model::disable_device(Device const &device)
{
	with_io_mmu(device.name(), [&] (Io_mmu &io_mmu) {
		destroy(_heap, &io_mmu); });

	with_irq_controller(device.name(), [&] (auto &irq_controller) {
		destroy(_heap, &irq_controller); });
}

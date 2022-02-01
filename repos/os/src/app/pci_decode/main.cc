/*
 * \brief  PCI configuration space decoder
 * \author Stefan Kalkowski
 * \date   2021-12-12
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <os/reporter.h>

#include <irq.h>
#include <pci/config.h>

using namespace Genode;
using namespace Pci;


struct Main
{
	Env                  & env;
	Heap                   heap            { env.ram(), env.rm()       };
	Attached_rom_dataspace platform_info   { env, "platform_info"      };
	Attached_rom_dataspace sys_rom         { env, "system"             };
	Signal_handler<Main>   sys_rom_handler { env.ep(), *this,
	                                         &Main::sys_rom_update     };
	Expanding_reporter     pci_reporter    { env, "devices", "devices" };
	Registry<Bridge>       bridge_registry {}; /* contains host bridges */

	unsigned msi_number { 0U };

	bool apic_capable { false };
	bool msi_capable  { false };

	List_model<Irq_routing>  irq_routing_list  {};
	List_model<Irq_override> irq_override_list {};

	Constructible<Attached_io_mem_dataspace> pci_config_ds {};

	void parse_pci_function(Bdf bdf, Config & cfg,
	                        addr_t cfg_phys_base,
	                        Xml_generator & generator);
	void parse_pci_bus(bus_t bus, bus_t offset, addr_t base,
	                   addr_t phys_base, Xml_generator & generator);

	void parse_irq_override_rules(Xml_node & xml);
	void parse_pci_config_spaces(Xml_node & xml);
	void sys_rom_update();

	template <typename FN>
	void for_bridge(Pci::bus_t bus, FN const & fn)
	{
		bridge_registry.for_each([&] (Bridge & b) {
			if (b.behind(bus)) b.find_bridge(bus, fn); });
	}

	Main(Env & env);
};


void Main::parse_pci_function(Bdf             bdf,
                              Config        & cfg,
                              addr_t          cfg_phys_base,
                              Xml_generator & generator)
{
	cfg.scan();

	Config::Vendor::access_t vendor = cfg.read<Config::Vendor>();
	Config::Device::access_t device = cfg.read<Config::Device>();
	Config::Header_type::Type::access_t type =
		cfg.read<Config::Header_type::Type>();
	Config::Class_code_rev_id::Class_code::access_t dclass =
		cfg.read<Config::Class_code_rev_id::Class_code>();

	if (type) {
		for_bridge(bdf.bus, [&] (Bridge & parent) {
			Config_type1 bcfg(cfg.base());
			new (heap) Bridge(parent.sub_bridges, bdf,
			                  bcfg.secondary_bus_number(),
			                  bcfg.subordinate_bus_number());
		});
	}

	bool      msi     = cfg.msi_cap.constructed();
	bool      msi_x   = cfg.msi_x_cap.constructed();
	irq_pin_t irq_pin = cfg.read<Config::Irq_pin>();

	generator.node("device", [&]
	{
		generator.attribute("name", Bdf::string(bdf));
		generator.attribute("type", "pci");

		generator.node("pci-config", [&]
		{
		generator.attribute("address",   String<16>(Hex(cfg_phys_base)));
		generator.attribute("bus",       String<16>(Hex(bdf.bus)));
		generator.attribute("device",    String<16>(Hex(bdf.dev)));
		generator.attribute("function",  String<16>(Hex(bdf.fn)));
		generator.attribute("vendor_id", String<16>(Hex(vendor)));
		generator.attribute("device_id", String<16>(Hex(device)));
		generator.attribute("class",     String<16>(Hex(dclass)));
		generator.attribute("bridge",    cfg.bridge() ? "yes" : "no");
		});

		cfg.for_each_bar([&] (uint64_t addr, size_t size) {
			generator.node("io_mem", [&]
			{
				generator.attribute("address", String<16>(Hex(addr)));
				generator.attribute("size",    String<16>(Hex(size)));
			});
		}, [&] (uint64_t addr, size_t size) {
			generator.node("io_port_range", [&]
			{
				generator.attribute("address", String<16>(Hex(addr)));
				generator.attribute("size",    String<16>(Hex(size)));
			});
		});


		/* IRQ pins count from 1-4 (INTA-D), zero means no IRQ defined */
		if (!irq_pin)
			return;

		generator.node("irq", [&]
		{
			if (msi_capable && msi_x) {
				generator.attribute("type", "msi-x");
				generator.attribute("number", msi_number++);
				return;
			}
			
			if (msi_capable && msi) {
				generator.attribute("type", "msi");
				generator.attribute("number", msi_number++);
				return;
			}

			irq_line_t irq = cfg.read<Config::Irq_line>();

			for_bridge(bdf.bus, [&] (Bridge & b) {
				irq_routing_list.for_each([&] (Irq_routing & ir) {
					ir.route(b, bdf.dev, irq_pin-1, irq); });
			});

			irq_override_list.for_each([&] (Irq_override & io) {
				io.generate(generator, irq); });

			generator.attribute("number", irq);
		});
	});
}


void Main::parse_pci_bus(bus_t           bus,
                         bus_t           offset,
                         addr_t          base,
                         addr_t          phys_base,
                         Xml_generator & generator)
{
	auto per_function = [&] (addr_t config_base, addr_t config_phys_base,
	                         dev_t dev, func_t fn) {
		Config cfg(config_base);
		if (!cfg.valid())
			return true;

		parse_pci_function({(bus_t)(bus+offset), dev, fn}, cfg,
		                   config_phys_base, generator);

		return !(fn == 0 && !cfg.read<Config::Header_type::Multi_function>());
	};

	for (dev_t dev = 0; dev < DEVICES_PER_BUS_MAX; dev++) {
		for (func_t fn = 0; fn < FUNCTION_PER_DEVICE_MAX; fn++) {
			unsigned factor = (bus * DEVICES_PER_BUS_MAX + dev) *
			                  FUNCTION_PER_DEVICE_MAX + fn;
			addr_t config_base = base + factor * FUNCTION_CONFIG_SPACE_SIZE;
			addr_t config_phys_base =
				phys_base + factor * FUNCTION_CONFIG_SPACE_SIZE;
			if (!per_function(config_base, config_phys_base, dev, fn))
				break;
		}
	}
}


void Main::parse_pci_config_spaces(Xml_node & xml)
{
	pci_reporter.generate([&] (Xml_generator & generator)
	{
		unsigned host_bridge_num = 0;

		xml.for_each_sub_node("bdf", [&] (Xml_node & xml)
		{
			addr_t const start = xml.attribute_value("start",  0UL);
			addr_t const base  = xml.attribute_value("base",   0UL);
			size_t const count = xml.attribute_value("count",  0UL);

			bus_t const bus_off   = (bus_t) (start / FUNCTION_PER_BUS_MAX);
			bus_t const bus_count = (bus_t) (count / FUNCTION_PER_BUS_MAX);

			if (host_bridge_num++) {
				error("We do not support multiple host bridges by now!");
				return;
			}

			new (heap) Bridge(bridge_registry, { bus_off, 0, 0 },
			                  bus_off, bus_count);

			pci_config_ds.construct(env, base, count * FUNCTION_CONFIG_SPACE_SIZE);

			for (bus_t bus = 0; bus < bus_count; bus++)
				parse_pci_bus((bus_t)bus, bus_off,
				              (addr_t)pci_config_ds->local_addr<void>(),
				              base, generator);

			pci_config_ds.destruct();
		});
	});
}


void Main::sys_rom_update()
{
	sys_rom.update();

	Xml_node xml = sys_rom.xml();

	if (apic_capable) {
		Irq_override_policy policy(heap);
		irq_override_list.update_from_xml(policy, xml);
	}

	if (apic_capable) {
		Irq_routing_policy policy(heap);
		irq_routing_list.update_from_xml(policy, xml);
	}

	parse_pci_config_spaces(xml);
}


Main::Main(Env & env) : env(env)
{
	sys_rom.sigh(sys_rom_handler);
	platform_info.xml().with_sub_node("kernel", [&] (Xml_node xml)
	{
		apic_capable = xml.attribute_value("acpi", false);
		msi_capable  = xml.attribute_value("msi",  false);
	});
}


void Component::construct(Genode::Env &env) { static Main main(env); }

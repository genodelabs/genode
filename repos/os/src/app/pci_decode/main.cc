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
#include <base/attached_io_mem_dataspace.h>

#include <irq.h>
#include <rmrr.h>
#include <pci/config.h>

using namespace Genode;
using namespace Pci;


struct Main
{
	Env                  & env;
	Heap                   heap            { env.ram(), env.rm()       };
	Attached_rom_dataspace platform_info   { env, "platform_info"      };
	Expanding_reporter     pci_reporter    { env, "devices", "devices", { 32*1024 } };
	Registry<Bridge>       bridge_registry {}; /* contains host bridges */

	bool apic_capable { false };
	bool msi_capable  { false };

	List_model<Irq_routing>  irq_routing_list  {};
	List_model<Irq_override> irq_override_list {};
	List_model<Rmrr>         reserved_memory_list {};

	Constructible<Attached_io_mem_dataspace> pci_config_ds {};

	bus_t parse_pci_function(Bdf bdf, Config & cfg,
	                         addr_t cfg_phys_base,
	                         Xml_generator & generator, unsigned & msi);
	bus_t parse_pci_bus(bus_t bus, addr_t base, addr_t phys_base,
	                    Xml_generator & generator, unsigned & msi);

	void parse_irq_override_rules(Xml_node & xml);
	void parse_pci_config_spaces(Xml_node & xml, Xml_generator & generator);
	void parse_acpi_device_info(Xml_node const &xml, Xml_generator & generator);
	void parse_tpm2_table(Xml_node const &xml, Xml_generator & gen);

	template <typename FN>
	void for_bridge(Pci::bus_t bus, FN const & fn)
	{
		bridge_registry.for_each([&] (Bridge & b) {
			if (b.behind(bus)) b.find_bridge(bus, fn); });
	}

	Main(Env & env);
};


/*
 * The bus and function parsers return either the current bus number or the
 * subordinate bus number (highest bus number of all of the busses that can be
 * reached downstream of a bridge).
 */

bus_t Main::parse_pci_function(Bdf             bdf,
                               Config        & cfg,
                               addr_t          cfg_phys_base,
                               Xml_generator & gen,
                               unsigned      & msi_number)
{
	cfg.scan();

	bus_t subordinate_bus = bdf.bus;

	/* check for bridges */
	if (cfg.read<Config::Header_type::Type>()) {
		for_bridge(bdf.bus, [&] (Bridge & parent) {
			Config_type1 bcfg(cfg.base());
			new (heap) Bridge(parent.sub_bridges, bdf,
			                  bcfg.secondary_bus_number(),
			                  bcfg.subordinate_bus_number());

			subordinate_bus = bcfg.subordinate_bus_number();

			/* enable I/O spaces and DMA in bridges if not done already */
			using Command = Pci::Config::Command;
			Command::access_t command = bcfg.read<Command>();
			if (Command::Io_space_enable::get(command)     == 0 ||
			    Command::Memory_space_enable::get(command) == 0 ||
			    Command::Bus_master_enable::get(command)   == 0) {
				Command::Io_space_enable::set(command, 1);
				Command::Memory_space_enable::set(command, 1);
				Command::Bus_master_enable::set(command, 1);
				bcfg.write<Command>(command);
			}
		});
	}

	bool      msi     = cfg.msi_cap.constructed();
	bool      msi_x   = cfg.msi_x_cap.constructed();
	irq_pin_t irq_pin = cfg.read<Config::Irq_pin>();

	gen.node("device", [&]
	{
		auto string = [&] (uint64_t v) { return String<16>(Hex(v)); };

		gen.attribute("name", Bdf::string(bdf));
		gen.attribute("type", "pci");

		gen.node("pci-config", [&]
		{
			using C  = Config;
			using C0 = Config_type0;
			using Cc = Config::Class_code_rev_id;

			gen.attribute("address",       string(cfg_phys_base));
			gen.attribute("bus",           string(bdf.bus));
			gen.attribute("device",        string(bdf.dev));
			gen.attribute("function",      string(bdf.fn));
			gen.attribute("vendor_id",     string(cfg.read<C::Vendor>()));
			gen.attribute("device_id",     string(cfg.read<C::Device>()));
			gen.attribute("class",         string(cfg.read<Cc::Class_code>()));
			gen.attribute("revision",      string(cfg.read<Cc::Revision>()));
			gen.attribute("bridge",        cfg.bridge() ? "yes" : "no");
			if (!cfg.bridge()) {
				C0 cfg0(cfg.base());
				gen.attribute("sub_vendor_id",
				              string(cfg0.read<C0::Subsystem_vendor>()));
				gen.attribute("sub_device_id",
				              string(cfg0.read<C0::Subsystem_device>()));
			}
		});

		cfg.for_each_bar([&] (uint64_t addr, uint64_t size,
		                      unsigned bar, bool pf)
		{
			gen.node("io_mem", [&]
			{
				gen.attribute("pci_bar", bar);
				gen.attribute("address", string(addr));
				gen.attribute("size",    string(size));
				if (pf) gen.attribute("prefetchable", true);
			});
		}, [&] (uint64_t addr, uint64_t size, unsigned bar) {
			gen.node("io_port_range", [&]
			{
				gen.attribute("pci_bar", bar);
				gen.attribute("address", string(addr));

				/* on x86 I/O ports can be in range 0-64KB only */
				gen.attribute("size", string(size & 0xffff));
			});
		});

		{
			/* Apply GSI/MSI/MSI-X quirks based on vendor/device/class */
			using Cc = Config::Class_code_rev_id;

			bool const hdaudio = cfg.read<Cc::Class_code>() == 0x40300;
			auto const vendor_id = cfg.read<Config::Vendor>();
			auto const device_id = cfg.read<Config::Device>();

			if (hdaudio && vendor_id == 0x1022 /* AMD */) {
				/**
				 * see dde_bsd driver dev/pci/azalia.c
				 *
				 * PCI_PRODUCT_AMD_17_HDA
				 * PCI_PRODUCT_AMD_17_1X_HDA
				 * PCI_PRODUCT_AMD_HUDSON2_HDA
				 */
				if (device_id == 0x1457 || device_id == 0x15e3 ||
				    device_id == 0x780d)
					msi = msi_x = false;
			}

			/*
			 * Force use of GSI on given ath9k device as using MSI
			 * does not work.
			 */
			if (vendor_id == 0x168c || device_id == 0x0034)
				msi = msi_x = false;
		}

		/*
		 * Only generate <irq> nodes if at least one of the following
		 * options is operational.
		 *
		 * - An IRQ pin from 1-4 (INTA-D) specifies legacy IRQ or GSI can be
		 *   used, zero means no IRQ defined.
		 * - The used platform/kernel is MSI-capable and the device includes an
		 *   MSI/MSI-X PCI capability.
		 *
		 * An <irq> node advertises (in decreasing priority) MSI-X, MSI, or
		 * legacy/GSI exclusively.
		 */
		bool const supports_irq = irq_pin != 0;
		bool const supports_msi = msi_capable && (msi_x || msi);

		if (supports_irq || supports_msi)
			gen.node("irq", [&]
			{
				if (msi_capable && msi) {
					gen.attribute("type", "msi");
					gen.attribute("number", msi_number++);
					return;
				}

				if (msi_capable && msi_x) {
					gen.attribute("type", "msi-x");
					gen.attribute("number", msi_number++);
					return;
				}

				irq_line_t irq = cfg.read<Config::Irq_line>();

				for_bridge(bdf.bus, [&] (Bridge & b) {
					irq_routing_list.for_each([&] (Irq_routing & ir) {
						ir.route(b, bdf.dev, irq_pin-1, irq); });
				});

				irq_override_list.for_each([&] (Irq_override & io) {
					io.generate(gen, irq); });

				gen.attribute("number", irq);
			});

		reserved_memory_list.for_each([&] (Rmrr & rmrr) {
			if (rmrr.bdf == bdf)
				gen.node("reserved_memory", [&]
				{
					gen.attribute("address", rmrr.addr);
					gen.attribute("size",    rmrr.size);
				});
		});
	});

	return subordinate_bus;
}


bus_t Main::parse_pci_bus(bus_t           bus,
                          addr_t          base,
                          addr_t          phys_base,
                          Xml_generator & generator,
                          unsigned      & msi_number)
{
	bus_t max_subordinate_bus = bus;

	auto per_function = [&] (addr_t config_base, addr_t config_phys_base,
	                         dev_t dev, func_t fn) {
		Config cfg(config_base);
		if (!cfg.valid())
			return true;

		bus_t const subordinate_bus =
			parse_pci_function({(bus_t)bus, dev, fn}, cfg,
			                   config_phys_base, generator, msi_number);

		max_subordinate_bus = max(max_subordinate_bus, subordinate_bus);

		return !(fn == 0 && !cfg.read<Config::Header_type::Multi_function>());
	};

	for (dev_t dev = 0; dev < DEVICES_PER_BUS_MAX; dev++) {
		for (func_t fn = 0; fn < FUNCTION_PER_DEVICE_MAX; fn++) {
			unsigned factor = dev * FUNCTION_PER_DEVICE_MAX + fn;
			addr_t config_base = base + factor * FUNCTION_CONFIG_SPACE_SIZE;
			addr_t config_phys_base =
				phys_base + factor * FUNCTION_CONFIG_SPACE_SIZE;
			if (!per_function(config_base, config_phys_base, dev, fn))
				break;
		}
	}

	return max_subordinate_bus;
}


static void parse_acpica_info(Xml_node const &xml, Xml_generator &gen)
{
	gen.node("device", [&] {
		gen.attribute("name", "acpi");
		gen.attribute("type", "acpi");

		xml.with_optional_sub_node("sci_int", [&] (Xml_node xml) {
			gen.node("irq", [&] {
				gen.attribute("number", xml.attribute_value("irq", 0xff));
			});
		});
	});
}

/*
 * Parse the TPM2 ACPI table and report the device if available.
 * Only CRB devices are supported at this time.
 *
 * See the following document for further information:
 * https://trustedcomputinggroup.org/wp-content/uploads/TCG_ACPIGeneralSpec_v1p3_r8_pub.pdf
 */
void Main::parse_tpm2_table(Xml_node const &xml, Xml_generator & gen)
{
	enum {
		TPM2_TABLE_CRB_ADDRESS_OFFSET = 40,
		TPM2_TABLE_CRB_ADDRESS_MASK = (~0xfff),
		TPM2_TABLE_START_METHOD_OFFSET = 48,
		TPM2_TABLE_START_METHOD_CRB = 7,
		TPM2_TABLE_MIN_SIZE = 52UL,
		TPM2_DEVICE_IO_MEM_SIZE = 0x1000U,
	};

	addr_t const addr = xml.attribute_value("addr",   0UL);
	size_t const size = xml.attribute_value("size",  0UL);

	if ((addr < 1UL) || (size < TPM2_TABLE_MIN_SIZE)) {
		error("TPM2 table info invalid");
		return;
	}

	Attached_io_mem_dataspace io_mem { env, addr, size };
	char* ptr = io_mem.local_addr<char>();

	if (memcmp(ptr, "TPM2", 4) != 0) {
		error("TPM2 table parse error");
		return;
	}

	uint32_t start_method =
		*(reinterpret_cast<uint32_t*>(ptr + TPM2_TABLE_START_METHOD_OFFSET));
	if (start_method != TPM2_TABLE_START_METHOD_CRB) {
		warning("Unsupported TPM2 device found");
		return;
	}

	addr_t crb_address =
		*(reinterpret_cast<addr_t*>(ptr + TPM2_TABLE_CRB_ADDRESS_OFFSET)) &
		TPM2_TABLE_CRB_ADDRESS_MASK;

	gen.node("device", [&]
	{
		gen.attribute("name", "tpm2");
		gen.node("io_mem", [&] {
			gen.attribute("address", crb_address);
			gen.attribute("size", TPM2_DEVICE_IO_MEM_SIZE);
		});
	});
}

/*
 * By now, we do not have the necessary information about non-PCI devices
 * available from the ACPI tables, therefore we hard-code typical devices
 * we assume to be found in this function. In the future, this function
 * shall interpret ACPI tables information.
 */
void Main::parse_acpi_device_info(Xml_node const &xml, Xml_generator & gen)
{
	using Table_name = String<5>;

	xml.for_each_sub_node("table", [&] (Xml_node &table) {
		Table_name name = table.attribute_value("name", Table_name());
		/* only the TPM2 table is supported at this time */
		if (name == "TPM2") {
			parse_tpm2_table(table, gen);
		}
	});

	/*
	 * PS/2 device
	 */
	gen.node("device", [&]
	{
		gen.attribute("name", "ps2");
		gen.node("irq", [&] { gen.attribute("number", 1U); });
		gen.node("irq", [&] { gen.attribute("number", 12U); });
		gen.node("io_port_range", [&]
		{
			gen.attribute("address", "0x60");
			gen.attribute("size", 1U);
		});
		gen.node("io_port_range", [&]
		{
			gen.attribute("address", "0x64");
			gen.attribute("size", 1U);
		});
	});

	/*
	 * PIT device
	 */
	gen.node("device", [&]
	{
		gen.attribute("name", "pit");
		gen.node("irq", [&] { gen.attribute("number", 0U); });
		gen.node("io_port_range", [&]
		{
			gen.attribute("address", "0x40");
			gen.attribute("size", 4U);
		});
	});

	/*
	 * ACPI device (if applicable)
	 */
	if (xml.has_sub_node("sci_int"))
		parse_acpica_info(xml, gen);
}


void Main::parse_pci_config_spaces(Xml_node & xml, Xml_generator & generator)
{
	/*
	 * We count beginning from 1 not 0, because some clients (Linux drivers)
	 * do not ignore the pseudo MSI number announced, but interpret zero as
	 * invalid.
	 */
	unsigned msi_number      = 1;
	unsigned host_bridge_num = 0;

	xml.for_each_sub_node("bdf", [&] (Xml_node & xml)
	{
		addr_t const start = xml.attribute_value("start",  0UL);
		addr_t const base  = xml.attribute_value("base",   0UL);
		size_t const count = xml.attribute_value("count",  0UL);

		bus_t const bus_off  = (bus_t) (start / FUNCTION_PER_BUS_MAX);
		bus_t const last_bus = (bus_t)
			(max(1UL, (count / FUNCTION_PER_BUS_MAX)) - 1);

		if (host_bridge_num++) {
			error("We do not support multiple host bridges by now!");
			return;
		}

		new (heap) Bridge(bridge_registry, { bus_off, 0, 0 },
		                  bus_off, last_bus);

		bus_t bus = 0;
		bus_t max_subordinate_bus = bus;
		do {
			enum { BUS_SIZE = DEVICES_PER_BUS_MAX * FUNCTION_PER_DEVICE_MAX
			                  * FUNCTION_CONFIG_SPACE_SIZE };
			addr_t offset = base + bus * BUS_SIZE;
			pci_config_ds.construct(env, offset, BUS_SIZE);
			bus_t const subordinate_bus =
				parse_pci_bus((bus_t)bus + bus_off,
				              (addr_t)pci_config_ds->local_addr<void>(),
				              offset, generator, msi_number);

			max_subordinate_bus = max(max_subordinate_bus, subordinate_bus);
		} while (bus++ < max_subordinate_bus);

		pci_config_ds.destruct();
	});
}


Main::Main(Env & env) : env(env)
{
	platform_info.xml().with_optional_sub_node("kernel", [&] (Xml_node xml)
	{
		apic_capable = xml.attribute_value("acpi", false);
		msi_capable  = xml.attribute_value("msi",  false);
	});

	Attached_rom_dataspace sys_rom(env, "system");
	sys_rom.update();

	/*
	 * Wait until the system ROM is available
	 */
	if (!sys_rom.valid()) {
		struct Io_dummy { void fn() {}; } io_dummy;
		Io_signal_handler<Io_dummy> handler(env.ep(), io_dummy, &Io_dummy::fn);
		sys_rom.sigh(handler);
		while (!sys_rom.valid()) {
			env.ep().wait_and_dispatch_one_io_signal();
			sys_rom.update();
		}
	}

	Xml_node xml = sys_rom.xml();

	if (apic_capable) {

		update_list_model_from_xml(irq_override_list, xml,

			/* create */
			[&] (Xml_node const &node) -> Irq_override &
			{
				return *new (heap)
					Irq_override(node.attribute_value<uint8_t>("irq",   0xff),
					             node.attribute_value<uint8_t>("gsi",   0xff),
					             node.attribute_value<uint8_t>("flags", 0));
			},

			/* destroy */
			[&] (Irq_override &irq_override) { destroy(heap, &irq_override); },

			/* update */
			[&] (Irq_override &, Xml_node const &) { }
		);

		update_list_model_from_xml(irq_routing_list, xml,

			/* create */
			[&] (Xml_node const &node) -> Irq_routing &
			{
				rid_t bridge_bdf = node.attribute_value<rid_t>("bridge_bdf", 0xff);
				return *new (heap)
					Irq_routing(Bdf::bdf(bridge_bdf),
					            node.attribute_value<uint8_t>("device",     0xff),
					            node.attribute_value<uint8_t>("device_pin", 0xff),
					            node.attribute_value<uint8_t>("gsi",        0xff));
			},

			/* destroy */
			[&] (Irq_routing &irq_routing) { destroy(heap, &irq_routing); },

			/* update */
			[&] (Irq_routing &, Xml_node const &) { }
		);
	}

	update_list_model_from_xml(reserved_memory_list, xml,

		/* create */
		[&] (Xml_node const &node) -> Rmrr &
		{
			bus_t  bus = 0;
			dev_t  dev = 0;
			func_t fn  = 0;
			addr_t start = node.attribute_value("start", 0UL);
			addr_t end   = node.attribute_value("end", 0UL);

			node.with_optional_sub_node("scope", [&] (Xml_node node) {
				bus = node.attribute_value<uint8_t>("bus_start", 0U);
				node.with_optional_sub_node("path", [&] (Xml_node node) {
					dev = node.attribute_value<uint8_t>("dev", 0);
					fn  = node.attribute_value<uint8_t>("func", 0);
				});
			});

			return *new (heap) Rmrr({bus, dev, fn}, start, (end-start+1));
		},

		/* destroy */
		[&] (Rmrr &rmrr) { destroy(heap, &rmrr); },

		/* update */
		[&] (Rmrr &, Xml_node const &) { }
	);

	pci_reporter.generate([&] (Xml_generator & generator)
	{
		parse_acpi_device_info(xml, generator);
		parse_pci_config_spaces(xml, generator);
	});
}


void Component::construct(Genode::Env &env) { static Main main(env); }

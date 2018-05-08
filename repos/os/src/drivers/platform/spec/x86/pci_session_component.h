/*
 * \brief  Platform session component
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

/* base */
#include <base/allocator_guard.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/rpc_server.h>
#include <base/tslab.h>
#include <ram_session/capability.h>
#include <root/component.h>

#include <util/mmio.h>
#include <util/retry.h>
#include <util/reconstructible.h>

/* os */
#include <io_mem_session/connection.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <platform_session/platform_session.h>
#include <base/allocator_guard.h>

/* local */
#include "device_pd.h"
#include "pci_bridge.h"
#include "pci_config_access.h"
#include "pci_device_component.h"

typedef Genode::Ram_dataspace_capability Ram_capability;

namespace Platform {
	unsigned short bridge_bdf(unsigned char bus);

	class Pci_buses;
	class Ram_dataspace;
	class Rmrr;
	class Root;
	class Session_component;
}

class Platform::Ram_dataspace : public Genode::List<Ram_dataspace>::Element {

	private:

		Ram_capability const _cap;

	public:
		Ram_dataspace(Ram_capability c) : _cap(c) { }

		bool match(const Ram_capability &cap) const {
			return cap.local_name() == _cap.local_name(); }

		Ram_capability cap() const { return _cap; }
};

class Platform::Rmrr : public Genode::List<Platform::Rmrr>::Element
{
	public:

		class Bdf : public Genode::List<Bdf>::Element {

			private:

				Genode::uint8_t _bus, _dev, _func;

			public:

				Bdf(Genode::uint8_t bus, Genode::uint8_t dev,
				    Genode::uint8_t func)
				: _bus(bus), _dev(dev), _func(func) { }

				bool match(Genode::uint8_t bus, Genode::uint8_t dev,
				           Genode::uint8_t func) {
					return bus == _bus && dev == _dev && func == _func; }
		};

	private:

		Genode::uint64_t const _start, _end;

		Genode::Io_mem_dataspace_capability _cap { };

		Genode::List<Bdf> _bdf_list { };

		Genode::Constructible<Genode::Io_mem_connection> _io_mem { };

	public:

		Rmrr(Genode::uint64_t start, Genode::uint64_t end)
		: _start(start), _end(end)
		{ }

		Genode::Io_mem_dataspace_capability match(Genode::Env &env,
		                                          Device_config config)
		{
			Genode::uint8_t bus      = config.bus_number();
			Genode::uint8_t device   = config.device_number();
			Genode::uint8_t function = config.function_number();

			for (Bdf *bdf = _bdf_list.first(); bdf; bdf = bdf->next()) {
				if (!bdf->match(bus, device, function))
					continue;

				if (_cap.valid())
					return _cap;

				_io_mem.construct(env, _start, _end - _start + 1);
				_cap = _io_mem->dataspace();
				return _cap;
			}
			return Genode::Io_mem_dataspace_capability();
		}

		void add(Bdf * bdf) { _bdf_list.insert(bdf); }

		static Genode::List<Rmrr> *list()
		{
			static Genode::List<Rmrr> _list;
			return &_list;
		}
};


class Platform::Pci_buses
{
	private:

		Genode::Bit_array<Device_config::MAX_BUSES> _valid { };

		void scan_bus(Config_access &config_access, Genode::Allocator &heap,
		              unsigned char bus = 0);

		bool _bus_valid(int bus)
		{
			if (bus >= Device_config::MAX_BUSES)
				return false;

			return _valid.get(bus, 1);
		}

	public:

		Pci_buses(Genode::Allocator &heap, Genode::Attached_io_mem_dataspace &pciconf)
		{
			Config_access c(pciconf);
			scan_bus(c, heap);
		}

		/**
		 * Scan PCI buses for a device
		 *
		 * \param bus                start scanning at bus number
		 * \param device             start scanning at device number
		 * \param function           start scanning at function number
		 * \param out_device_config  device config information of the
		 *                           found device
		 * \param config_access      interface for accessing the PCI
		 *                           configuration
		 *                           space
		 *
		 * \retval true   device was found
		 * \retval false  no device was found
		 */
		bool find_next(int bus, int device, int function,
		               Device_config *out_device_config,
		               Config_access *config_access)
		{
			for (; bus < Device_config::MAX_BUSES; bus++) {
				if (!_bus_valid(bus))
					continue;

				for (; device < Device_config::MAX_DEVICES; device++) {
					for (; function < Device_config::MAX_FUNCTIONS; function++) {

						/* read config space */
						Device_config config(bus, device, function, config_access);

						if (config.valid()) {
							*out_device_config = config;
							return true;
						}
					}
					function = 0; /* init value for next device */
				}
				device = 0; /* init value for next bus */
			}
			return false;
		}
};


class Platform::Session_component : public Genode::Rpc_object<Session>
{
	private:

		Genode::Env                       &_env;
		Genode::Attached_rom_dataspace    &_config;
		Genode::Attached_io_mem_dataspace &_pciconf;
		Genode::Ram_quota_guard            _ram_guard;
		Genode::Cap_quota_guard            _cap_guard;
		Genode::Constrained_ram_allocator  _env_ram {
			_env.pd(), _ram_guard, _cap_guard };
		Genode::Heap                       _md_alloc;
		Genode::Session_label       const  _label;
		Genode::Session_policy      const  _policy { _label, _config.xml() };
		Genode::List<Device_component>     _device_list { };
		Platform::Pci_buses               &_pci_bus;
		Genode::Heap                      &_global_heap;
		bool                               _iommu;

		/**
		 * Registry of RAM dataspaces allocated by the session
		 */
		Genode::List<Platform::Ram_dataspace> _ram_caps { };

		void _insert(Ram_capability cap) {
			_ram_caps.insert(new (_md_alloc) Platform::Ram_dataspace(cap)); }

		bool _remove(Ram_capability cap)
		{
			for (Platform::Ram_dataspace *ds = _ram_caps.first(); ds;
			     ds = ds->next()) {

				if (!ds->match(cap))
					continue;

				_ram_caps.remove(ds);
				destroy(_md_alloc, ds);
				return true;
			}
			return false;
		}

		Platform::Device_pd _device_pd { _env, _label, _ram_guard, _cap_guard };

		enum { MAX_PCI_DEVICES = Device_config::MAX_BUSES *
		                         Device_config::MAX_DEVICES *
		                         Device_config::MAX_FUNCTIONS };

		static Genode::Bit_array<MAX_PCI_DEVICES> bdf_in_use;

		/**
		 * List containing extended PCI config space information
		 */
		static Genode::List<Config_space> &config_space_list()
		{
			static Genode::List<Config_space> config_space;
			return config_space;
		}

		/**
		 * Find for a given PCI device described by the bus:dev:func triple
		 * the corresponding extended 4K PCI config space address.
		 * A io mem dataspace is created and returned.
		 */
		Genode::addr_t lookup_config_space(Genode::uint16_t const bdf)
		{
			using namespace Genode;

			addr_t config_space = ~0UL; /* invalid */

			Config_space *e = config_space_list().first();
			for (; e && (config_space == ~0UL); e = e->next())
				config_space = e->lookup_config_space(bdf);

			return config_space;
		}

		/*
		 * List of aliases for PCI Class/Subclas/Prog I/F triple used
		 * by xml config for this platform driver
		 */
		unsigned class_subclass_prog(const char * name)
		{
			using namespace Genode;

			static struct {
				const char * alias;
				uint8_t pci_class, pci_subclass, pci_progif;
			} const aliases [] = {
				{ "AHCI"     , 0x1, 0x06, 0x0},
				{ "ALL"      , 0x0, 0x00, 0x0},
				{ "AUDIO"    , 0x4, 0x01, 0x0},
				{ "ETHERNET" , 0x2, 0x00, 0x0},
				{ "HDAUDIO"  , 0x4, 0x03, 0x0},
				{ "NVME"     , 0x1, 0x08, 0x2},
				{ "USB"      , 0xc, 0x03, 0x0},
				{ "VGA"      , 0x3, 0x00, 0x0},
				{ "WIFI"     , 0x2, 0x80, 0x0},
				{ "ISABRIDGE", 0x6, 0x01, 0x0}
			};

			for (unsigned i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
				if (strcmp(aliases[i].alias, name))
					continue;

				return 0U | aliases[i].pci_class << 16 |
				       aliases[i].pci_subclass << 8 |
				       aliases[i].pci_progif;
			}

			return ~0U;
		}

		/**
		 * Check device usage according to session policy
		 */
		bool permit_device(const char * name)
		{
			using namespace Genode;

			try {
				_policy.for_each_sub_node("device", [&] (Xml_node dev) {
					try {
						/* enforce restriction based on name name */
						char policy_name[8];
						dev.attribute("name").value(policy_name,
						                            sizeof(policy_name));

						if (!strcmp(policy_name, name))
							/* found identical match - permit access */
							throw true;

					} catch (Xml_attribute::Nonexistent_attribute) { }
				});
			} catch (bool result) { return result; }

			return false;
		}

		/**
		 * Check according session policy device usage
		 */
		bool permit_device(Genode::uint8_t b, Genode::uint8_t d,
		                   Genode::uint8_t f, unsigned class_code)
		{
			using namespace Genode;

			try {
				_policy.for_each_sub_node("pci", [&] (Xml_node node) {
					try {
						unsigned bus, device, function;

						node.attribute("bus").value(&bus);
						node.attribute("device").value(&device);
						node.attribute("function").value(&function);

						if (b == bus && d == device && f == function)
							throw true;

						return;
					} catch (Xml_attribute::Nonexistent_attribute) { }

					/* enforce restriction based upon classes */
					unsigned class_sub_prog = 0;

					try {
						char alias_class[32];
						node.attribute("class").value(alias_class,
						                              sizeof(alias_class));

						class_sub_prog = class_subclass_prog(alias_class);
					} catch (Xml_attribute::Nonexistent_attribute) {
						return;
					}

					enum { DONT_CHECK_PROGIF = 8 };
					/* if class/subclass don't match - deny */
					if (class_sub_prog && (class_sub_prog ^ class_code) >> DONT_CHECK_PROGIF)
						return;

					/* if this bdf is used by some policy - deny */
					if (find_dev_in_policy(b, d, f))
						return;

					throw true;
				});
			} catch (bool result) { return result; }

			return false;
		}

		/**
		 * Lookup a given device name.
		 */
		bool find_dev_in_policy(const char * dev_name, bool once = true)
		{
			using namespace Genode;

			try {
				_config.xml().for_each_sub_node("policy", [&] (Xml_node policy) {
					policy.for_each_sub_node("device", [&] (Xml_node device) {
						try {
							/* device attribute from policy node */
							char policy_device[8];
							device.attribute("name").value(policy_device, sizeof(policy_device));

							if (!strcmp(policy_device, dev_name)) {
								if (once)
									throw true;
								once = true;
							}
						} catch (Xml_node::Nonexistent_attribute) { }
					});
				});
			} catch (bool result) { return result; }

			return false;
		}

		/**
		 * Lookup a given device name.
		 */
		bool find_dev_in_policy(Genode::uint8_t b, Genode::uint8_t d,
		                        Genode::uint8_t f, bool once = true)
		{
			using namespace Genode;

			try {
				Xml_node xml = _config.xml();
				xml.for_each_sub_node("policy", [&] (Xml_node policy) {
					policy.for_each_sub_node("pci", [&] (Xml_node node) {
						try {
							unsigned bus, device, function;

							node.attribute("bus").value(&bus);
							node.attribute("device").value(&device);
							node.attribute("function").value(&function);

							if (b == bus && d == device && f == function) {
								if (once)
									throw true;
								once = true;
							}
						} catch (Xml_node::Nonexistent_attribute) { }
					});
				});
			} catch (bool result) { return result; }

			return false;
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::Env                       &env,
		                  Genode::Attached_rom_dataspace    &config,
		                  Genode::Attached_io_mem_dataspace &pciconf,
		                  Platform::Pci_buses               &buses,
		                  Genode::Heap                      &global_heap,
		                  char                        const *args,
		                  bool                        const iommu)
		:
			_env(env),
			_config(config),
			_pciconf(pciconf),
			_ram_guard(Genode::ram_quota_from_args(args)),
			_cap_guard(Genode::cap_quota_from_args(args)),
			_md_alloc(_env_ram, env.rm()),
			_label(Genode::label_from_args(args)),
			_pci_bus(buses), _global_heap(global_heap), _iommu(iommu)
		{
			/* subtract the RPC session and session dataspace capabilities */
			_cap_guard.withdraw(Genode::Cap_quota{2});

			/* non-pci devices */
			_policy.for_each_sub_node("device", [&] (Genode::Xml_node device_node) {
				try {
					char policy_device[8];
					device_node.attribute("name").value(policy_device,
					                                    sizeof(policy_device));

					enum { DOUBLET = false };
					if (!find_dev_in_policy(policy_device, DOUBLET))
						return;

					Genode::error("'", _label, "' - device "
					              "'", Genode::Cstring(policy_device), "' "
					              "is part of more than one policy");
				}
				catch (Genode::Xml_node::Nonexistent_attribute) {
					Genode::error("'", _label, "' - device node "
					              "misses a 'name' attribute");
				}
				throw Genode::Service_denied();
			});

			/* pci devices */
			_policy.for_each_sub_node("pci", [&] (Genode::Xml_node node) {
				enum { INVALID_CLASS = 0x1000000U };
				unsigned class_sub_prog = INVALID_CLASS;

				using Genode::Xml_attribute;

				/**
				 * Valid input is either a triple of 'bus', 'device',
				 * 'function' attributes or a single 'class' attribute.
				 * All other attribute names are traded as wrong.
				 */
				try {
					char alias_class[32];
					node.attribute("class").value(alias_class,
					                              sizeof(alias_class));

					class_sub_prog = class_subclass_prog(alias_class);
					if (class_sub_prog >= INVALID_CLASS) {
						Genode::error("'", _label, "' - invalid 'class' ",
						              "attribute '", Genode::Cstring(alias_class), "'");
						throw Genode::Service_denied();
					}
				} catch (Xml_attribute::Nonexistent_attribute) { }

				/* if we read a class attribute all is fine */
				if (class_sub_prog < INVALID_CLASS) {
					/* sanity check that 'class' is the only attribute */
					try {
						node.attribute(1);
						Genode::error("'", _label, "' - attributes beside 'class' detected");
						throw Genode::Service_denied();
					}
					catch (Xml_attribute::Nonexistent_attribute) { }

					/* we have a class and it is the only attribute */
					return;
				}

				/* no 'class' attribute - now check for valid bdf triple */
				try {
					node.attribute(3);
					Genode::error("'", _label, "' - "
					              "invalid number of pci node attributes");
					throw Genode::Service_denied();

				} catch (Xml_attribute::Nonexistent_attribute) { }

				try {
					unsigned bus, device, function;

					node.attribute("bus").value(&bus);
					node.attribute("device").value(&device);
					node.attribute("function").value(&function);

					if ((bus >= Device_config::MAX_BUSES) ||
					    (device >= Device_config::MAX_DEVICES) ||
					    (function >= Device_config::MAX_FUNCTIONS))
					throw Xml_attribute::Nonexistent_attribute();

					enum { DOUBLET = false };
					if (!find_dev_in_policy(bus, device, function, DOUBLET))
						return;

					Genode::error("'", _label, "' - device '",
					              Genode::Hex(bus), ":",
					              Genode::Hex(device), ".", function, "' "
					              "is part of more than one policy");

				} catch (Xml_attribute::Nonexistent_attribute) {
					Genode::error("'", _label, "' - "
					              "invalid pci node attributes for bdf");
				}
				throw Genode::Service_denied();
			});
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			/* release all elements of the session's device list */
			while (_device_list.first())
				release_device(_device_list.first()->cap());

			while (Platform::Ram_dataspace *ds = _ram_caps.first()) {
				_ram_caps.remove(ds);
				_env_ram.free(ds->cap());
				destroy(_md_alloc, ds);
			}
		}


		void upgrade_resources(Genode::Session::Resources resources)
		{
			_ram_guard.upgrade(resources.ram_quota);
			_cap_guard.upgrade(resources.cap_quota);
		}


		static void add_config_space(Genode::uint32_t bdf_start,
		                             Genode::uint32_t func_count,
		                             Genode::addr_t base,
		                             Genode::Allocator &heap)
		{
			using namespace Genode;
			Config_space * space =
				new (heap) Config_space(bdf_start, func_count, base);
			config_space_list().insert(space);
		}


		/**
		 * Check whether msi usage was explicitly switched off
		 */
		bool msi_usage()
		{
			try {
				char mode[8];
				_policy.attribute("irq_mode").value(mode, sizeof(mode));
				if (!Genode::strcmp("nomsi", mode))
					 return false;
			} catch (Genode::Xml_node::Nonexistent_attribute) { }

			return true;
		}


		/***************************
		 ** PCI session interface **
		 ***************************/

		Device_capability first_device(unsigned device_class,
		                               unsigned class_mask) override {
			return next_device(Device_capability(), device_class, class_mask); }

		Device_capability next_device(Device_capability prev_device,
		                              unsigned device_class,
		                              unsigned class_mask) override
		{
			/*
			 * Create the interface to the PCI config space.
			 */
			Config_access config_access(_pciconf);

			/* lookup device component for previous device */
			auto lambda = [&] (Device_component *prev)
			{

				/*
				 * Start bus scanning after the previous device's location.
				 * If no valid device was specified for 'prev_device',
				 * start at the beginning.
				 */
				int bus = 0, device = 0, function = -1;

				if (prev) {
					Device_config config = prev->config();
					bus      = config.bus_number();
					device   = config.device_number();
					function = config.function_number();
				}

				/*
				 * Scan buses for devices.
				 * If no device is found, return an invalid capability.
				 */
				Device_config config;

				while (true) {
					function += 1;
					if (!_pci_bus.find_next(bus, device, function, &config,
					                        &config_access))
						return Device_capability();

					/* get new bdf values */
					bus      = config.bus_number();
					device   = config.device_number();
					function = config.function_number();

					/* if filter of driver don't match skip and continue */
					if ((config.class_code() ^ device_class) & class_mask)
						continue;

					/* check that policy permit access to the matched device */
					if (permit_device(bus, device, function,
					    config.class_code()))
						break;
				}

				/* lookup if we have a extended pci config space */
				Genode::addr_t config_space =
					lookup_config_space(config.bdf());

				/*
				 * A device was found. Create a new device component for the
				 * device and return its capability.
				 */
				try {
					Device_component * dev = new (_md_alloc)
						Device_component(_env, config, config_space, config_access, *this,
						                 _md_alloc, _global_heap);

					/* if more than one driver uses the device - warn about */
					if (bdf_in_use.get(Device_config::MAX_BUSES * bus +
					    Device_config::MAX_DEVICES * device +
					    function, 1))
						Genode::error("Device ", config,
						              " is used by more than one driver - "
						              "session '", _label, "'.");
					else
						bdf_in_use.set(Device_config::MAX_BUSES * bus +
						               Device_config::MAX_DEVICES * device +
						               function, 1);

					_device_list.insert(dev);
					return _env.ep().rpc_ep().manage(dev);
				}
				catch (Genode::Out_of_ram) {
					throw;
				}
				catch (Genode::Out_of_caps) {
					Genode::warning("Out_of_caps during Device_component construction");
					throw;
				}
			};
			return _env.ep().rpc_ep().apply(prev_device, lambda);
		}

		void release_device(Device_capability device_cap) override
		{
			Device_component * device;
			auto lambda = [&] (Device_component *d)
			{
				device = d;
				if (!device)
					return;

				unsigned const bus  = device->config().bus_number();
				unsigned const dev  = device->config().device_number();
				unsigned const func = device->config().function_number();

				if (bdf_in_use.get(Device_config::MAX_BUSES * bus +
				                   Device_config::MAX_DEVICES * dev +
				                   func, 1))
					bdf_in_use.clear(Device_config::MAX_BUSES * bus +
					                 Device_config::MAX_DEVICES * dev + func, 1);

				_device_list.remove(device);
				_env.ep().rpc_ep().dissolve(device);
			};

			/* lookup device component for previous device */
			_env.ep().rpc_ep().apply(device_cap, lambda);

			if (device && device->config().valid())
				destroy(_md_alloc, device);
		}

		void assign_device(Device_component * device)
		{
			using namespace Genode;

			if (!device || device->config_space() == ~0UL || !_iommu)
				return;

			try {
				addr_t const function = device->config().bus_number() * 32 * 8 +
				                        device->config().device_number() * 8 +
				                        device->config().function_number();
				addr_t const base_ecam = Dataspace_client(_pciconf.cap()).phys_addr();
				addr_t const base_offset = 0x1000UL * function;

				if (base_ecam + base_offset != device->config_space())
					throw 1;

				_device_pd.assign_pci(_pciconf.cap(), base_offset, device->config().bdf());

				for (Rmrr *r = Rmrr::list()->first(); r; r = r->next()) {
					Io_mem_dataspace_capability rmrr_cap = r->match(_env, device->config());
					if (rmrr_cap.valid())
						_device_pd.attach_dma_mem(rmrr_cap);
				}
			} catch (...) {
				Genode::error("assignment to device pd or of RMRR region failed");
			}
		}

		/**
		 * De-/Allocation of dma capable dataspaces
		 */

		Ram_capability alloc_dma_buffer(Genode::size_t const size) override
		{
			Ram_capability ram_cap = _env_ram.alloc(size, Genode::UNCACHED);

			if (!ram_cap.valid())
				return ram_cap;

			try {
				_device_pd.attach_dma_mem(ram_cap);
				_insert(ram_cap);
			} catch (Out_of_ram)  {
				_env_ram.free(ram_cap);
				throw Genode::Out_of_ram();
			} catch (Out_of_caps) {
				_env_ram.free(ram_cap);
				throw Genode::Out_of_caps();
			}

			return ram_cap;
		}

		void free_dma_buffer(Ram_capability ram_cap) override
		{
			if (!ram_cap.valid() || !_remove(ram_cap))
				return;

			_env_ram.free(ram_cap);
		}

		Device_capability device(String const &name) override;
};


class Platform::Root : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env                    &_env;
		Genode::Attached_rom_dataspace &_config;

		Genode::Constructible<Genode::Attached_io_mem_dataspace> _pci_confspace { };

		Genode::Reporter _pci_reporter { _env, "pci" };

		Genode::Heap _heap { _env.ram(), _env.rm() };

		Genode::Constructible<Platform::Pci_buses> _buses { };

		bool _iommu { false };

		void _parse_report_rom(Genode::Env &env, const char * acpi_rom,
		                       bool acpi_platform)
		{
			using namespace Genode;

			Xml_node xml_acpi(acpi_rom);
			if (!xml_acpi.has_type("acpi"))
				throw 1;

			xml_acpi.for_each_sub_node("bdf", [&] (Xml_node &node) {

				uint32_t bdf_start  = 0;
				uint32_t func_count = 0;
				addr_t   base       = 0;

				node.attribute("start").value(&bdf_start);
				node.attribute("count").value(&func_count);
				node.attribute("base").value(&base);

				Session_component::add_config_space(bdf_start, func_count,
				                                    base, _heap);

				Device_config const bdf_first(bdf_start);
				Device_config const bdf_last(bdf_start + func_count - 1);
				addr_t const memory_size = 0x1000UL * func_count;

				/* Simplification: Only consider first config space and
				 * check if it is for domain 0 */
				if (bdf_start || _pci_confspace.constructed()) {
					warning("ECAM/MMCONF range ",
					        bdf_first, "-", bdf_last, " - addr ",
					        Hex_range<addr_t>(base, memory_size), " ignored");
					return;
				}

				log("ECAM/MMCONF range ", bdf_first, "-", bdf_last, " - addr ",
				    Hex_range<addr_t>(base, memory_size));

				_pci_confspace.construct(env, base, memory_size);
			});

			if (!_pci_confspace.constructed())
				throw 2;

			Config_access config_access(*_pci_confspace);

			for (unsigned i = 0; i < xml_acpi.num_sub_nodes(); i++) {
				Xml_node node = xml_acpi.sub_node(i);

				if (node.has_type("bdf"))
					continue;

				if (node.has_type("irq_override")) {
					unsigned irq = 0xff;
					unsigned gsi = 0xff;
					unsigned flags = 0xff;

					node.attribute("irq").value(&irq);
					node.attribute("gsi").value(&gsi);
					node.attribute("flags").value(&flags);

					if (!acpi_platform) {
						warning("MADT IRQ ", irq, "-> GSI ", gsi, " flags ",
						        flags, " ignored");
						continue;
					}

					using Platform::Irq_override;
					Irq_override * o = new (_heap) Irq_override(irq, gsi,
					                                            flags);
					Irq_override::list()->insert(o);
					continue;
				}

				if (node.has_type("drhd")) {
					_iommu = true;
					continue;
				}

				if (node.has_type("rmrr")) {
					uint64_t mem_start = 0, mem_end = 0;
					node.attribute("start").value(&mem_start);
					node.attribute("end").value(&mem_end);

					if (node.num_sub_nodes() == 0)
						throw 3;

					Rmrr * rmrr = new (_heap) Rmrr(mem_start, mem_end);
					Rmrr::list()->insert(rmrr);

					for (unsigned s = 0; s < node.num_sub_nodes(); s++) {
						Xml_node scope = node.sub_node(s);
						if (!scope.num_sub_nodes() || !scope.has_type("scope"))
							throw 4;

						unsigned bus = 0, dev = 0, func = 0;
						scope.attribute("bus_start").value(&bus);

						for (unsigned p = 0; p < scope.num_sub_nodes(); p++) {
							Xml_node path = scope.sub_node(p);
							if (!path.has_type("path"))
								throw 5;

							path.attribute("dev").value(&dev);
							path.attribute("func").value(&func);

							Device_config bridge(bus, dev, func,
							                     &config_access);
							if (bridge.pci_bridge())
								/* PCI bridge spec 3.2.5.3, 3.2.5.4 */
								bus = bridge.read(config_access, 0x19,
								                  Device::ACCESS_8BIT);
						}

						rmrr->add(new (_heap) Rmrr::Bdf(bus, dev, func));
					}
					continue;
				}

				if (node.has_type("root_bridge")) {
					node.attribute("bdf").value(&Platform::Bridge::root_bridge_bdf);
					continue;
				}

				if (!node.has_type("routing")) {
					error ("unsupported node '", node.type(), "'");
					throw __LINE__;
				}

				unsigned gsi;
				unsigned bridge_bdf;
				unsigned device;
				unsigned device_pin;

				node.attribute("gsi").value(&gsi);
				node.attribute("bridge_bdf").value(&bridge_bdf);
				node.attribute("device").value(&device);
				node.attribute("device_pin").value(&device_pin);

				/* drop routing information on non ACPI platform */
				if (!acpi_platform)
					continue;

				Irq_routing * r = new (_heap) Irq_routing(gsi, bridge_bdf,
				                                          device, device_pin);
				Irq_routing::list()->insert(r);
			}
		}

		void _construct_buses()
		{
			Genode::Dataspace_client ds_pci_mmio(_pci_confspace->cap());
			uint64_t const phys_addr = ds_pci_mmio.phys_addr();
			uint64_t const phys_size = ds_pci_mmio.size();
			uint64_t       mmio_size = 0x10000000UL; /* max MMCONF memory */

			/* try surviving wrong ACPI ECAM/MMCONF table information */
			while (true) {
				try {
					_buses.construct(_heap, *_pci_confspace);
					/* construction and scan succeeded */
					break;
				} catch (Platform::Config_access::Invalid_mmio_access) {

					error("ECAM/MMCONF MMIO access out of bounds - "
					      "ACPI table information is wrong!");

					_pci_confspace.destruct();

					while (mmio_size > phys_size) {
						try {
							error(" adjust size from ", Hex(phys_size),
							      "->", Hex(mmio_size));
							_pci_confspace.construct(_env, phys_addr, mmio_size);
							/* got memory - try again */
							break;
						} catch (Genode::Service_denied) {
							/* decrease by one bus memory size */
							mmio_size -= 0x1000UL * 32 * 8;
						}
					}
					if (mmio_size <= phys_size)
						/* broken machine - you're lost */
						throw;
				}
			}
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			try {
				return  new (md_alloc())
					Session_component(_env, _config, *_pci_confspace, *_buses,
					                  _heap, args, _iommu);
			}
			catch (Genode::Session_policy::No_policy_defined) {
				Genode::error("Invalid session request, no matching policy for ",
				              "'", Genode::label_from_args(args).string(), "'");
				throw Genode::Service_denied();
			}
		}


		void _upgrade_session(Session_component *s, const char *args) override {
			s->upgrade_resources(Genode::session_resources_from_args(args)); }

	public:

		/**
		 * Constructor
		 *
		 * \param ep        entry point to be used for serving the PCI session
		 *                  and PCI device interface
		 * \param md_alloc  meta-data allocator for allocating PCI-session
		 *                  components and PCI-device components
		 */
		Root(Genode::Env &env, Genode::Allocator &md_alloc,
		     Genode::Attached_rom_dataspace &config,
		     const char *acpi_rom,
		     bool acpi_platform)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(),
			                                          &md_alloc),
			_env(env), _config(config)
		{

			try {
				_parse_report_rom(env, acpi_rom, acpi_platform);
			} catch (...) {
				Genode::error("ACPI report parsing error.");
				throw;
			}

			if (Platform::Bridge::root_bridge_bdf < Platform::Bridge::INVALID_ROOT_BRIDGE) {
				Device_config config(Platform::Bridge::root_bridge_bdf);
				Genode::log("Root bridge: ", config);
			} else
				Genode::warning("Root bridge: unknown");

			_construct_buses();

			_pci_reporter.enabled(config.xml().has_sub_node("report") &&
			                      config.xml().sub_node("report")
			                                  .attribute_value("pci", true));

			if (_pci_reporter.enabled()) {

				Config_access config_access(*_pci_confspace);
				Device_config config;
				int bus = 0, device = 0, function = -1;

				Genode::Reporter::Xml_generator xml(_pci_reporter, [&] () {
					/* iterate over pci devices */
					while (true) {
						function += 1;
						if (!(*_buses).find_next(bus, device, function, &config,
						                         &config_access))
							return;

						bus      = config.bus_number();
						device   = config.device_number();
						function = config.function_number();

						using Genode::String;
						using Genode::Hex;

						xml.node("device", [&] () {
							xml.attribute("bdf"       , String<8>(Hex(bus, Hex::Prefix::OMIT_PREFIX), ":", Hex(device, Hex::Prefix::OMIT_PREFIX), ".", function));
							xml.attribute("vendor_id" , String<8>(Hex(config.vendor_id())));
							xml.attribute("device_id" , String<8>(Hex(config.device_id())));
							xml.attribute("class_code", String<12>(Hex(config.class_code())));
							xml.attribute("bridge"    , config.pci_bridge() ? "yes" : "no");
						});
					}
				});
			}
		}
};

/*
 * \brief  platform session component
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

/* base */
#include <base/allocator_guard.h>
#include <base/rpc_server.h>
#include <base/tslab.h>
#include <cap_session/connection.h>

#include <ram_session/connection.h>
#include <root/component.h>
#include <root/client.h>

#include <util/mmio.h>
#include <util/retry.h>
#include <util/volatile_object.h>

/* os */
#include <io_mem_session/connection.h>
#include <os/session_policy.h>
#include <platform_session/platform_session.h>

/* local */
#include "pci_device_component.h"
#include "pci_config_access.h"
#include "pci_device_pd_ipc.h"
#include "device_pd.h"

namespace Platform {
	bool bus_valid(int bus = 0);
	unsigned short bridge_bdf(unsigned char bus);

	class Rmrr;
	class Root;
	class Ram_dataspace;
}

class Platform::Ram_dataspace : public Genode::List<Ram_dataspace>::Element {

	private:
		Genode::Ram_dataspace_capability _cap;

	public:
		Ram_dataspace(Genode::Ram_dataspace_capability c) : _cap(c) { }

		bool match(const Genode::Ram_dataspace_capability &cap) const {
			return cap.local_name() == _cap.local_name(); }
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

		Genode::uint64_t _start, _end;
		Genode::Io_mem_dataspace_capability _cap;

		Genode::List<Bdf> _bdf_list;

	public:

		Rmrr(Genode::uint64_t start, Genode::uint64_t end)
		: _start(start), _end(end)
		{ }

		Genode::Io_mem_dataspace_capability match(Device_config config) {
			Genode::uint8_t bus      = config.bus_number();
			Genode::uint8_t device   = config.device_number();
			Genode::uint8_t function = config.function_number();

			for (Bdf *bdf = _bdf_list.first(); bdf; bdf = bdf->next()) {
				if (!bdf->match(bus, device, function))
					continue;

				if (_cap.valid())
					return _cap;

				Genode::Io_mem_connection io_mem(_start, _end - _start + 1);
				io_mem.on_destruction(Genode::Io_mem_connection::KEEP_OPEN);
				_cap = io_mem.dataspace();

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

namespace Platform {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Genode::Rpc_entrypoint                     *_ep;
			Genode::Allocator_guard                     _md_alloc;
			Genode::Tslab<Device_component, 4096 - 64>  _device_slab;
			Genode::List<Device_component>              _device_list;
			Genode::Rpc_entrypoint                     &_device_pd_ep;

			struct Resources {
				Genode::Ram_connection _ram;
				Genode::List<Platform::Ram_dataspace> _ram_caps;
				Genode::Tslab<Platform::Ram_dataspace, 4096 - 64>  _ram_caps_slab;

				Resources(Genode::Allocator_guard &md_alloc)
				:
					/* restrict physical address to 3G on 32bit */
					_ram("dma", 0, (sizeof(void *) == 4)
					               ? 0xc0000000UL : 0x100000000ULL),
					_ram_caps_slab(&md_alloc)
				{
					/* associate _ram session with platform_drv _ram session */
					_ram.ref_account(Genode::env()->ram_session_cap());

					/*
					 * Equip RAM session with initial quota to account for
					 * core-internal allocation meta-data overhead.
					 */
					enum { OVERHEAD = 4096 };
					Genode::env()->ram_session()->transfer_quota(_ram, OVERHEAD);
				}

				Genode::Ram_connection &ram() { return _ram; }

				void insert(Genode::Ram_dataspace_capability cap) {
					_ram_caps.insert(new (_ram_caps_slab) Platform::Ram_dataspace(cap)); }

				bool remove(Genode::Ram_dataspace_capability cap) {
					for (Platform::Ram_dataspace *ds = _ram_caps.first(); ds;
					     ds = ds->next())
						if (ds->match(cap))
							return true;
					return false;
				}

			} _resources;

			struct Devicepd {
				Device_pd_policy        *policy;
				Device_pd_client         child;
				Genode::Allocator_guard &_md_alloc;

				enum { DEVICE_PD_RAM_QUOTA = 180 * 4096 };

				Devicepd (Genode::Rpc_entrypoint &ep,
				          Genode::Allocator_guard &md_alloc,
				          Genode::Ram_session_capability ram_ref_cap,
				          const char * label)
				:
				  policy(nullptr),
				  child(Genode::reinterpret_cap_cast<Device_pd>(Genode::Native_capability())),
				  _md_alloc(md_alloc)
				{
					if (!md_alloc.withdraw(DEVICE_PD_RAM_QUOTA))
						throw Out_of_metadata();

					if (Genode::env()->ram_session()->transfer_quota(ram_ref_cap, DEVICE_PD_RAM_QUOTA)) {
						Genode::error("Transferring quota for device pd failed");
						md_alloc.upgrade(DEVICE_PD_RAM_QUOTA);
						throw Platform::Session::Fatal();
					}

					try {
						policy = new (md_alloc) Device_pd_policy(ep, ram_ref_cap, DEVICE_PD_RAM_QUOTA, label);

						using Genode::Session_capability;
						using Genode::Affinity;

						Session_capability session = Genode::Root_client(policy->root()).session("", Affinity());
						child = Device_pd_client(Genode::reinterpret_cap_cast<Device_pd>(session));
					} catch (Genode::Rom_connection::Rom_connection_failed) {
						Genode::warning("PCI device protection domain for IOMMU support "
						                "is not available");

						if (policy) {
							destroy(md_alloc, policy);
							policy = nullptr;
						}

						md_alloc.upgrade(DEVICE_PD_RAM_QUOTA);
					} catch (Genode::Allocator::Out_of_memory) {
						md_alloc.upgrade(DEVICE_PD_RAM_QUOTA);
						throw Out_of_metadata();
					} catch(...) {
						Genode::error("unknown exception");
						md_alloc.upgrade(DEVICE_PD_RAM_QUOTA);
						throw Platform::Session::Fatal();
					}
				}

				~Devicepd() {
					if (!policy)
						return;

					destroy(_md_alloc, policy);
					_md_alloc.upgrade(DEVICE_PD_RAM_QUOTA);
				}

				bool valid() { return policy && policy->root().valid() && child.valid(); }
			};

			Genode::Session_label                       _label;
			Genode::Session_policy                      _policy;

			Genode::Lazy_volatile_object<struct Devicepd> _device_pd;

			enum { MAX_PCI_DEVICES = Device_config::MAX_BUSES *
			                         Device_config::MAX_DEVICES *
			                         Device_config::MAX_FUNCTIONS };

			static Genode::Bit_array<MAX_PCI_DEVICES> bdf_in_use;


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
			bool _find_next(int bus, int device, int function,
			                Device_config *out_device_config,
			                Config_access *config_access)
			{
				for (; bus < Device_config::MAX_BUSES; bus++) {
					if (!bus_valid(bus))
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

			/**
			 * List containing extended PCI config space information
			 */
			static Genode::List<Config_space> &config_space_list() {
				static Genode::List<Config_space> config_space;
				return config_space;
			}

			/**
			 * Find for a given PCI device described by the bus:dev:func triple
			 * the corresponding extended 4K PCI config space address.
			 * A io mem dataspace is created and returned.
			 */
			Genode::addr_t
			lookup_config_space(Genode::uint16_t const bdf)
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
			unsigned class_subclass_prog(const char * name) {
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
					config()->xml_node().for_each_sub_node("policy", [&] (Xml_node policy) {
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
					Xml_node xml(config()->xml_node());
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
			Session_component(Genode::Rpc_entrypoint  *ep,
			                  Genode::Allocator       *md_alloc,
			                  const char              *args,
			                  Genode::Rpc_entrypoint  &device_pd_ep)
			:
				_ep(ep),
				_md_alloc(md_alloc, Genode::Arg_string::find_arg(args, "ram_quota").long_value(0)),
				_device_slab(&_md_alloc),
				_device_pd_ep(device_pd_ep),
				_resources(_md_alloc),
				_label(Genode::label_from_args(args)),
				_policy(_label)
			{
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
					} catch (Genode::Xml_node::Nonexistent_attribute) {
						Genode::error("'", _label, "' - device node "
						              "misses a 'name' attribute");
					}
					throw Genode::Root::Unavailable();
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
							throw Genode::Root::Unavailable();
						}
					} catch (Xml_attribute::Nonexistent_attribute) { }

					/* if we read a class attribute all is fine */
					if (class_sub_prog < INVALID_CLASS) {
						/* sanity check that 'class' is the only attribute */
						try {
							node.attribute(1);
							Genode::error("'", _label, "' - attributes beside 'class' detected");
							throw Genode::Root::Unavailable();
						} catch (Xml_attribute::Nonexistent_attribute) { }

						/* we have a class and it is the only attribute */
						return;
					}

					/* no 'class' attribute - now check for valid bdf triple */
					try {
						node.attribute(3);
						Genode::error("'", _label, "' - "
						              "invalid number of pci node attributes");
						throw Genode::Root::Unavailable();
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
					throw Genode::Root::Unavailable();
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
			}


			void upgrade_ram_quota(long quota) { _md_alloc.upgrade(quota); }


			static void add_config_space(Genode::uint32_t bdf_start,
			                             Genode::uint32_t func_count,
			                             Genode::addr_t base)
			{
				using namespace Genode;
				Config_space * space =
					new (env()->heap()) Config_space(bdf_start, func_count,
					                                 base);
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
			                               unsigned class_mask) {
				return next_device(Device_capability(), device_class, class_mask); }

			Device_capability next_device(Device_capability prev_device,
			                              unsigned device_class,
			                              unsigned class_mask)
			{
				/*
				 * Create the interface to the PCI config space.
				 * This involves the creation of I/O port sessions.
				 */
				Config_access config_access;

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
						if (!_find_next(bus, device, function, &config,
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
						Device_component * dev = new (_device_slab)
							Device_component(config, config_space, _ep, this,
							                 &_md_alloc);

						/* if more than one driver uses the device - warn about */
						if (bdf_in_use.get(Device_config::MAX_BUSES * bus +
						    Device_config::MAX_DEVICES * device +
						    function, 1))
							Genode::error("Device ",
							              Genode::Hex(bus), ":",
							              Genode::Hex(device), ".", function, " "
							              "is used by more than one driver - "
							              "session '", _label, "'.");
						else
							bdf_in_use.set(Device_config::MAX_BUSES * bus +
							               Device_config::MAX_DEVICES * device +
							               function, 1);

						_device_list.insert(dev);
						return _ep->manage(dev);
					} catch (Genode::Allocator::Out_of_memory) {
						throw Out_of_metadata();
					}
				};
				return _ep->apply(prev_device, lambda);
			}

			void release_device(Device_capability device_cap)
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
					_ep->dissolve(device);
				};

				/* lookup device component for previous device */
				_ep->apply(device_cap, lambda);

				if (!device) return;

				if (device->config().valid())
					destroy(_device_slab, device);
				else
					destroy(_md_alloc, device);
			}

			void assign_device(Device_component * device)
			{
				using namespace Genode;

				if (!device || !device->get_config_space().valid())
					return;

				Io_mem_dataspace_capability io_mem = device->get_config_space();

				if (!_device_pd.constructed())
					_device_pd.construct(_device_pd_ep, _md_alloc,
					                     _resources.ram().cap(),
					                     _label.string());

				if (!_device_pd->valid())
					return;

				try {
					_device_pd->child.assign_pci(io_mem, device->config().bdf());

					for (Rmrr *r = Rmrr::list()->first(); r; r = r->next()) {
						Io_mem_dataspace_capability rmrr_cap = r->match(device->config());
						if (rmrr_cap.valid())
							_device_pd->child.attach_dma_mem(rmrr_cap);
					}
				} catch (...) {
					Genode::error("assignment to device pd or of RMRR region failed");
				}
			}

			/**
			 * De-/Allocation of dma capable dataspaces
			 */
			typedef Genode::Ram_dataspace_capability Ram_capability;


			/**
			 * Helper method for rollback
			 */
			void _rollback(const Genode::size_t size,
			               const Ram_capability ram_cap = Ram_capability(),
			               const bool throw_oom = true)
			{
				if (ram_cap.valid())
					_resources.ram().free(ram_cap);

				if (_resources.ram().transfer_quota(Genode::env()->ram_session_cap(), size))
					throw Fatal();

				_md_alloc.upgrade(size);

				if (throw_oom)
					throw Out_of_metadata();
			}

			Ram_capability alloc_dma_buffer(Genode::size_t const size)
			{
				if (!_device_pd.constructed())
					_device_pd.construct(_device_pd_ep, _md_alloc,
					                     _resources.ram().cap(),
					                     _label.string());

				if (!_md_alloc.withdraw(size))
					throw Out_of_metadata();

				/* transfer ram quota to session specific ram session */
				Genode::Ram_session * const rs = Genode::env()->ram_session();
				if (rs->transfer_quota(_resources.ram().cap(), size)) {
					_md_alloc.upgrade(size);
					throw Fatal();
				}

				enum { UPGRADE_QUOTA = 4096 };

				/* allocate dataspace from session specific ram session */
				Ram_capability ram_cap = Genode::retry<Genode::Ram_session::Quota_exceeded>(
					[&] () {
						Ram_capability ram = Genode::retry<Genode::Ram_session::Out_of_metadata>(
							[&] () { return _resources.ram().alloc(size, Genode::UNCACHED); },
							[&] () { throw Genode::Ram_session::Quota_exceeded(); });
						return ram;
					},
					[&] () {
						if (!_md_alloc.withdraw(UPGRADE_QUOTA))
							_rollback(size);

						char buf[32];
						Genode::snprintf(buf, sizeof(buf), "ram_quota=%u",
						                 UPGRADE_QUOTA);
						Genode::env()->parent()->upgrade(_resources.ram().cap(), buf);
					});

				if (!ram_cap.valid())
					return ram_cap;

				if (_device_pd->valid()) {
					Genode::retry<Genode::Rm_session::Out_of_metadata>(
						[&] () { _device_pd->child.attach_dma_mem(ram_cap); },
						[&] () {
							if (!_md_alloc.withdraw(UPGRADE_QUOTA))
								_rollback(size, ram_cap);

							if (rs->transfer_quota(_resources.ram().cap(), UPGRADE_QUOTA))
								throw Fatal();

							Genode::Ram_connection &slave = _device_pd->policy->ram_slave();
							if (_resources.ram().transfer_quota(slave.cap(), UPGRADE_QUOTA))
								throw Fatal();
						});
				}

				try {
					_resources.insert(ram_cap);
				} catch(Genode::Allocator::Out_of_memory) {
					_rollback(size, ram_cap);
				}
				return ram_cap;
			}

			void free_dma_buffer(Ram_capability ram_cap)
			{
				if (!ram_cap.valid() || !_resources.remove(ram_cap))
					return;

				Genode::size_t size = Genode::Dataspace_client(ram_cap).size();
				_rollback(size, ram_cap, false);
			}

			Device_capability device(String const &name) override;
	};
}

class Platform::Root : public Genode::Root_component<Session_component>
{
	private:

		struct Fadt {
			Genode::uint32_t features = 0, reset_type = 0, reset_value = 0;
			Genode::uint64_t reset_addr = 0;

			/* Table 5-35 Fixed ACPI Description Table Fixed Feature Flags */
			struct Features : Genode::Register<32> {
				struct Reset : Bitfield<10, 1> { };
			};

			/* ACPI spec - 5.2.3.2 Generic Address Structure */
			struct Gas : Genode::Register<32>
			{
				struct Address_space : Bitfield <0, 8> {
					enum { SYSTEM_IO = 1 };
				};
				struct Access_size : Bitfield<24,8> {
					enum { UNDEFINED = 0, BYTE = 1, WORD = 2, DWORD = 3, QWORD = 4};
				};
			};
		} fadt;

		Genode::Rpc_entrypoint _device_pd_ep;

		void _parse_report_rom(const char * acpi_rom)
		{
			using namespace Genode;

			Config_access config_access;

			Xml_node xml_acpi(acpi_rom);
			if (!xml_acpi.has_type("acpi"))
				throw 1;

			for (unsigned i = 0; i < xml_acpi.num_sub_nodes(); i++) {
				Xml_node node = xml_acpi.sub_node(i);

				if (node.has_type("bdf")) {

					uint32_t bdf_start  = 0;
					uint32_t func_count = 0;
					addr_t   base       = 0;

					node.attribute("start").value(&bdf_start);
					node.attribute("count").value(&func_count);
					node.attribute("base").value(&base);

					Session_component::add_config_space(bdf_start, func_count,
					                                    base);
				}

				if (node.has_type("irq_override")) {
					unsigned irq = 0xff;
					unsigned gsi = 0xff;
					unsigned flags = 0xff;

					node.attribute("irq").value(&irq);
					node.attribute("gsi").value(&gsi);
					node.attribute("flags").value(&flags);

					using Platform::Irq_override;
					Irq_override * o = new (env()->heap()) Irq_override(irq,
					                                                    gsi,
					                                                    flags);
					Irq_override::list()->insert(o);
				}

				if (node.has_type("rmrr")) {
					uint64_t mem_start, mem_end;
					node.attribute("start").value(&mem_start);
					node.attribute("end").value(&mem_end);

					if (node.num_sub_nodes() == 0)
						throw 2;

					Rmrr * rmrr = new (env()->heap()) Rmrr(mem_start, mem_end);
					Rmrr::list()->insert(rmrr);

					for (unsigned s = 0; s < node.num_sub_nodes(); s++) {
						Xml_node scope = node.sub_node(s);
						if (!scope.num_sub_nodes() || !scope.has_type("scope"))
							throw 3;

						unsigned bus, dev, func;
						scope.attribute("bus_start").value(&bus);

						for (unsigned p = 0; p < scope.num_sub_nodes(); p++) {
							Xml_node path = scope.sub_node(p);
							if (!path.has_type("path"))
								throw 4;

							path.attribute("dev").value(&dev);
							path.attribute("func").value(&func);

							Device_config bridge(bus, dev, func,
							                     &config_access);
							if (bridge.pci_bridge())
								/* PCI bridge spec 3.2.5.3, 3.2.5.4 */
								bus = bridge.read(&config_access, 0x19,
								                  Device::ACCESS_8BIT);
						}

						rmrr->add(new (env()->heap()) Rmrr::Bdf(bus, dev,
						                                        func));
					}
				}

				if (node.has_type("fadt")) {
					node.attribute("features").value(&fadt.features);
					node.attribute("reset_type").value(&fadt.reset_type);
					node.attribute("reset_addr").value(&fadt.reset_addr);
					node.attribute("reset_value").value(&fadt.reset_value);
				}

				if (!node.has_type("routing"))
					continue;

				unsigned gsi;
				unsigned bridge_bdf;
				unsigned device;
				unsigned device_pin;

				node.attribute("gsi").value(&gsi);
				node.attribute("bridge_bdf").value(&bridge_bdf);
				node.attribute("device").value(&device);
				node.attribute("device_pin").value(&device_pin);

				/* check that bridge bdf is actually a valid device */
				Device_config config((bridge_bdf >> 8 & 0xff),
				                     (bridge_bdf >> 3) & 0x1f,
				                      bridge_bdf & 0x7, &config_access);

				if (!config.valid())
					continue;

				if (!config.pci_bridge() && bridge_bdf != 0)
					/**
					 * If the bridge bdf has not a type header of a bridge in
					 * the pci config space, then it should be the host bridge
					 * device. The host bridge device need not to be
					 * necessarily at 0:0.0, it may be on another location. The
					 * irq routing information for the host bridge however
					 * contain entries for the bridge bdf to be 0:0.0 -
					 * therefore we override it here for the irq rerouting
					 * information of host bridge devices.
					 */
					bridge_bdf = 0;

				Irq_routing * r = new (env()->heap()) Irq_routing(gsi,
				                                                  bridge_bdf,
				                                                  device,
				                                                  device_pin);
				Irq_routing::list()->insert(r);
			}
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			try {
				return new (md_alloc()) Session_component(ep(), md_alloc(),
				                                          args, _device_pd_ep);
			} catch (Genode::Session_policy::No_policy_defined) {
				Genode::error("Invalid session request, no matching policy for ",
				              "'", Genode::label_from_args(args).string(), "'");
				throw Genode::Root::Unavailable();
			}
		}


		void _upgrade_session(Session_component *s, const char *args) override
		{
			long ram_quota = Genode::Arg_string::find_arg(args, "ram_quota").long_value(0);
			s->upgrade_ram_quota(ram_quota);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep        entry point to be used for serving the PCI session
		 *                  and PCI device interface
		 * \param md_alloc  meta-data allocator for allocating PCI-session
		 *                  components and PCI-device components
		 */
		Root(Genode::Env &env, Genode::Allocator *md_alloc,
		     const char *acpi_rom)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(),
			                                          md_alloc),
			_device_pd_ep(&env.pd(), STACK_SIZE, "device_pd_slave")
		{
			/* enforce initial bus scan */
			bus_valid();

			if (acpi_rom) {
				try {
					_parse_report_rom(acpi_rom);
				} catch (...) {
					Genode::error("PCI config space data could not be parsed.");
				}
			}
		}

		void system_reset()
		{
			const bool io_port_space = (Fadt::Gas::Address_space::get(fadt.reset_type) == Fadt::Gas::Address_space::SYSTEM_IO);

			if (!io_port_space)
				return;

			Config_access config_access;
			const unsigned raw_access_size = Fadt::Gas::Access_size::get(fadt.reset_type);
			const bool reset_support = config_access.reset_support(fadt.reset_addr, raw_access_size);
			if (!reset_support)
				return;

			const bool feature_reset = Fadt::Features::Reset::get(fadt.features);

			if (!feature_reset) {
				Genode::warning("system reset failed - feature not supported");
				return;
			}

			Device::Access_size access_size = Device::ACCESS_8BIT;

			unsigned raw_size = Fadt::Gas::Access_size::get(fadt.reset_type);
			switch (raw_size) {
			case Fadt::Gas::Access_size::WORD:
				access_size = Device::ACCESS_16BIT;
				break;
			case Fadt::Gas::Access_size::DWORD:
				access_size = Device::ACCESS_32BIT;
				break;
			case Fadt::Gas::Access_size::QWORD:
				Genode::error("system reset failed - unsupported access size");
				return;
			default:
				break;
			}

			config_access.system_reset(fadt.reset_addr, fadt.reset_value,
			                           access_size);
			/* if we are getting here - the reset failed */
			Genode::warning("system reset failed");
		}
};

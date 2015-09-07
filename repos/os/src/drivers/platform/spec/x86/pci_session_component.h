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

#include <ram_session/connection.h>
#include <root/component.h>
#include <root/client.h>

/* os */
#include <io_mem_session/connection.h>
#include <os/session_policy.h>
#include <platform_session/platform_session.h>

/* local */
#include "pci_device_component.h"
#include "pci_config_access.h"
#include "pci_device_pd_ipc.h"

namespace Platform {
	bool bus_valid(int bus = 0);
	unsigned short bridge_bdf(unsigned char bus);
}

namespace Platform {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Genode::Rpc_entrypoint                     *_ep;
			Genode::Allocator_guard                     _md_alloc;
			Genode::Tslab<Device_component, 4096 - 64>  _device_slab;
			Genode::List<Device_component>              _device_list;
			Device_pd_client                            _child;
			Genode::Ram_connection                     *_ram;
			Genode::Session_label                       _label;
			Genode::Session_policy                      _policy;

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
			lookup_config_space(Genode::uint8_t bus, Genode::uint8_t dev,
			                    Genode::uint8_t func)
			{
				using namespace Genode;

				uint32_t bdf = (bus << 8) | ((dev & 0x1f) << 3) | (func & 0x7);
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
			                  Genode::Root_capability &device_pd_root,
			                  Genode::Ram_connection  *ram,
			                  const char              *args)
			:
				_ep(ep),
				_md_alloc(md_alloc, Genode::Arg_string::find_arg(args, "ram_quota").long_value(0)),
				_device_slab(&_md_alloc), _child(Genode::reinterpret_cap_cast<Device_pd>(Genode::Native_capability())),
				_ram(ram), _label(args), _policy(_label)
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

						PERR("'%s' - device '%s' is part of more than one policy",
						     _label.string(), policy_device);
					} catch (Genode::Xml_node::Nonexistent_attribute) {
						PERR("'%s' - device node misses a 'name' attribute",
						     _label.string());
					}
					throw Genode::Root::Unavailable();
				});

				if (device_pd_root.valid()) {
					using Genode::Session_capability;
					using Genode::Affinity;

					Session_capability session = Genode::Root_client(device_pd_root).session("", Affinity());
					Genode::Capability <Device_pd> cap = Genode::reinterpret_cap_cast<Device_pd>(session);
					_child = Device_pd_client(cap);
				}

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
							PERR("'%s' - invalid 'class' attribute '%s'",
							     _label.string(), alias_class);
							throw Genode::Root::Unavailable();
						}
					} catch (Xml_attribute::Nonexistent_attribute) { }

					/* if we read a class attribute all is fine */
					if (class_sub_prog < INVALID_CLASS) {
						/* sanity check that 'class' is the only attribute */
						try {
							node.attribute(1);
							PERR("'%s' - attributes beside 'class' detected",
							     _label.string());
							throw Genode::Root::Unavailable();
						} catch (Xml_attribute::Nonexistent_attribute) { }

						/* we have a class and it is the only attribute */
						return;
					}

					/* no 'class' attribute - now check for valid bdf triple */
					try {
						node.attribute(3);
						PERR("'%s' - invalid number of pci node attributes",
						     _label.string());
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

						PERR("'%s' - device '%2x:%2x.%x' is part of more than "
						     "one policy", _label.string(), bus, device,
						     function);
					} catch (Xml_attribute::Nonexistent_attribute) {
						PERR("'%s' - invalid pci node attributes for bdf",
						     _label.string());
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
						lookup_config_space(bus, device, function);

					/*
					 * A device was found. Create a new device component for the
					 * device and return its capability.
					 */
					try {
						Device_component * dev = new (_device_slab)
							Device_component(config, config_space, _ep, this,
							                 msi_usage());

						/* if more than one driver uses the device - warn about */
						if (bdf_in_use.get(Device_config::MAX_BUSES * bus +
						    Device_config::MAX_DEVICES * device +
						    function, 1))
							PERR("Device %2x:%2x.%u is used by more than one "
							     "driver - session '%s'.", bus, device, function,
							     _label.string());
						else
							bdf_in_use.set(Device_config::MAX_BUSES * bus +
							               Device_config::MAX_DEVICES * device +
							               function, 1);

						_device_list.insert(dev);
						return _ep->manage(dev);
					} catch (Genode::Allocator::Out_of_memory) {
						throw Device::Quota_exceeded();
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

			Genode::Io_mem_dataspace_capability assign_device(Device_component * device)
			{
				using namespace Genode;

				if (!device || !device->get_config_space().valid())
					return Io_mem_dataspace_capability();

				Io_mem_dataspace_capability io_mem = device->get_config_space();

				if (_child.valid())
					_child.assign_pci(io_mem);

				/*
				 * By now forbid usage of extended pci config space dataspace,
				 * - until required.
				 */
				// return io_mem;
				return Io_mem_dataspace_capability();
			}

			Genode::Io_mem_dataspace_capability config_extended(Device_capability device_cap)
			{
				using namespace Genode;

				return _ep->apply(device_cap, [&] (Device_component *device) {
					return assign_device(device);});
			}

			/**
			 * De-/Allocation of dma capable dataspaces
			 */
			typedef Genode::Ram_dataspace_capability Ram_capability;

			Ram_capability alloc_dma_buffer(Genode::size_t size)
			{
				if (!_md_alloc.withdraw(size))
					throw Device::Quota_exceeded();

				Genode::Ram_session * const rs = Genode::env()->ram_session();
				if (rs->transfer_quota(_ram->cap(), size)) {
					_md_alloc.upgrade(size);
					return Ram_capability();
				}

				Ram_capability ram_cap;
				try {

					ram_cap = _ram->alloc(size, Genode::UNCACHED);
				} catch (Genode::Ram_session::Quota_exceeded) {
					_md_alloc.upgrade(size);
					return Ram_capability();
				}

				if (!ram_cap.valid() || !_child.valid())
					return ram_cap;

				_child.attach_dma_mem(ram_cap);

				return ram_cap;
			}

			void free_dma_buffer(Ram_capability ram)
			{
				if (ram.valid())
					_ram->free(ram);
			}

			Device_capability device(String const &name) override;
	};


	class Root : public Genode::Root_component<Session_component>
	{
		private:

			Genode::Root_capability _device_pd_root;
			/* Ram_session for allocation of dma capable dataspaces */
			Genode::Ram_connection _ram;

			void _parse_report_rom(const char * acpi_rom)
			{
				using namespace Genode;

				Config_access config_access;

				try {
					Xml_node xml_acpi(acpi_rom);
					if (!xml_acpi.has_type("acpi"))
						throw 1;

					unsigned i;

					for (i = 0; i < xml_acpi.num_sub_nodes(); i++)
					{
						Xml_node node = xml_acpi.sub_node(i);

						if (node.has_type("bdf")) {

							uint32_t bdf_start  = 0;
							uint32_t func_count = 0;
							addr_t   base       = 0;

							node.attribute("start").value(&bdf_start);
							node.attribute("count").value(&func_count);
							node.attribute("base").value(&base);

							Session_component::add_config_space(bdf_start,
							                                    func_count,
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
							Irq_override::list()->insert(new (env()->heap()) Irq_override(irq, gsi, flags));
						}

						if (node.has_type("routing")) {
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
							                      bridge_bdf & 0x7,
							                     &config_access);

							if (config.valid()) {
								if (!config.is_pci_bridge() && bridge_bdf != 0)
									/**
									 * If the bridge bdf has not a type header
									 * of a bridge in the pci config space,
									 * then it should be the host bridge
									 * device. The host bridge device need not
									 * to be necessarily at 0:0.0, it may be
									 * on another location. The irq routing
									 * information for the host bridge however
									 * contain entries for the bridge bdf to be
									 * 0:0.0 - therefore we override it here
									 * for the irq rerouting information of
									 * host bridge devices.
									 */
									bridge_bdf = 0;

								Irq_routing::list()->insert(new (env()->heap()) Irq_routing(gsi, bridge_bdf, device, device_pin));
							}
						}
					}
				} catch (...) {
					PERR("PCI config space data could not be parsed.");
				}
			}

		protected:

			Session_component *_create_session(const char *args)
			{
				try {
					return new (md_alloc()) Session_component(ep(), md_alloc(),
					                                          _device_pd_root,
					                                          &_ram, args);
				} catch (Genode::Session_policy::No_policy_defined) {
					PERR("Invalid session request, no matching policy for '%s'",
					     Genode::Session_label(args).string());
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
			 * \param ep        entry point to be used for serving the PCI session and
			 *                  PCI device interface
			 * \param md_alloc  meta-data allocator for allocating PCI-session
			 *                  components and PCI-device components
			 */
			Root(Genode::Rpc_entrypoint *ep, Genode::Allocator *md_alloc,
			     Genode::size_t pci_device_pd_ram_quota,
			     Genode::Root_capability &device_pd_root,
			     const char *acpi_rom)
			:
				Genode::Root_component<Session_component>(ep, md_alloc),
				_device_pd_root(device_pd_root),
				/* restrict physical address to 3G on 32bit with device_pd */
				_ram("dma", 0, (device_pd_root.valid() && sizeof(void *) == 4) ?
				               0xc0000000UL : 0x100000000ULL)
			{
				/* enforce initial bus scan */
				bus_valid();

				if (acpi_rom)
					_parse_report_rom(acpi_rom);

				/* associate _ram session with ram_session of process */
				_ram.ref_account(Genode::env()->ram_session_cap());
				Genode::env()->ram_session()->transfer_quota(_ram.cap(), 0x1000);
			}
	};

}

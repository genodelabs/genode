/*
 * \brief  PCI-session component
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PCI_SESSION_COMPONENT_H_
#define _PCI_SESSION_COMPONENT_H_

/* base */
#include <base/allocator_guard.h>
#include <base/rpc_server.h>
#include <base/tslab.h>

#include <ram_session/connection.h>
#include <root/component.h>

/* os */
#include <io_mem_session/connection.h>
#include <os/config.h>
#include <os/session_policy.h>
#include <pci_session/pci_session.h>

/* local */
#include "pci_device_component.h"
#include "pci_config_access.h"
#include "pci_device_pd_ipc.h"

namespace Pci {
	bool bus_valid(int bus = 0);
}

namespace Pci {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Genode::Rpc_entrypoint                     *_ep;
			Genode::Allocator_guard                     _md_alloc;
			Genode::Tslab<Device_component, 4096 - 64>  _device_slab;
			Genode::List<Device_component>              _device_list;
			Device_pd_client                           *_child;
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
					{ "AHCI"    , 0x1, 0x06, 0x0},
					{ "ALL"     , 0x0, 0x00, 0x0},
					{ "ETHERNET", 0x2, 0x00, 0x0},
					{ "USB"     , 0xc, 0x03, 0x0},
					{ "VGA"     , 0x3, 0x00, 0x0},
					{ "WIFI"    , 0x2, 0x80, 0x0}
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

			/*
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

			/*
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
						/* if this does not identical matches - deny access */
						if ((class_sub_prog & class_code) != class_sub_prog)
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
			Session_component(Genode::Rpc_entrypoint *ep,
			                  Genode::Allocator      *md_alloc,
			                  Device_pd_client       *child,
			                  Genode::Ram_connection *ram,
			                  const char             *args)
			:
				_ep(ep),
				_md_alloc(md_alloc, Genode::Arg_string::find_arg(args, "ram_quota").long_value(0)),
				_device_slab(&_md_alloc),
				_child(child), _ram(ram), _label(args), _policy(_label)
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
				Genode::Object_pool<Device_component>::Guard
					prev(_ep->lookup_and_lock(prev_device));

				/*
				 * Start bus scanning after the previous device's location.
				 * If no valid device was specified for 'prev_device', start at
				 * the beginning.
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
					if (!_find_next(bus, device, function, &config, &config_access))
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
				Genode::addr_t config_space = lookup_config_space(bus, device,
				                                                  function);

				/*
				 * A device was found. Create a new device component for the
				 * device and return its capability.
				 */
				try {
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

					Device_component * dev = new (_device_slab) Device_component(config, config_space, _ep, this);
					_device_list.insert(dev);
					return _ep->manage(dev);
				} catch (Genode::Allocator::Out_of_memory) {
					throw Device::Quota_exceeded();
				}
			}

			void release_device(Device_capability device_cap)
			{
				/* lookup device component for previous device */
				Device_component *device = dynamic_cast<Device_component *>
				                           (_ep->lookup_and_lock(device_cap));

				if (!device)
					return;

				unsigned const bus  = device->config().bus_number();
				unsigned const dev  = device->config().device_number();
				unsigned const func = device->config().function_number();

				bdf_in_use.clear(Device_config::MAX_BUSES * bus +
				                 Device_config::MAX_DEVICES * dev + func, 1);

				_device_list.remove(device);
				_ep->dissolve(device);

				Genode::Io_mem_connection * io_mem = device->get_config_space();
				if (io_mem)
					destroy(_md_alloc, io_mem);

				if (device->config().valid())
					destroy(_device_slab, device);
				else
					destroy(_md_alloc, device);
			}

			Genode::Io_mem_dataspace_capability config_extended(Device_capability device_cap)
			{
				using namespace Genode;

				Object_pool<Device_component>::Guard
					device(_ep->lookup_and_lock(device_cap));

				if (!device || device->config_space() == ~0UL)
					return Io_mem_dataspace_capability();

				Io_mem_connection * io_mem = device->get_config_space();
				if (io_mem)
					return io_mem->dataspace();

				try {
					io_mem = new (_md_alloc) Io_mem_connection(device->config_space(),
					                                           0x1000);
				} catch (Genode::Allocator::Out_of_memory) {
					throw Device::Quota_exceeded();
				} catch (Parent::Service_denied) {
					return Io_mem_dataspace_capability();
				}

				device->set_config_space(io_mem);

				if (_child)
					_child->assign_pci(io_mem->dataspace());

				return io_mem->dataspace();
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

				if (!ram_cap.valid() || !_child)
					return ram_cap;

				_child->attach_dma_mem(ram_cap);

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

			/* for now we have only one device pd for all pci devices */
			Device_pd_client *_pd_device_client;
			/* Ram_session for allocation of dma capable dataspaces */
			Genode::Ram_connection _ram;

			void _parse_config()
			{
				using namespace Genode;

				/* check for config file first */
				try { config(); } catch (...) { return; }

				try {
					unsigned i;

					for (i = 0; i < config()->xml_node().num_sub_nodes(); i++)
					{
						Xml_node node = config()->xml_node().sub_node(i);

						if (!node.has_type("bdf"))
							continue;

						uint32_t bdf_start  = 0;
						uint32_t func_count = 0;
						addr_t   base       = 0;
						node.sub_node("start").value(&bdf_start);
						node.sub_node("count").value(&func_count);
						node.sub_node("base").value(&base);

						PINF("%2u BDF start %x, functions: 0x%x, physical base "
						     "0x%lx", i, bdf_start, func_count, base);

						Session_component::add_config_space(bdf_start,
						                                    func_count, base);
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
					                                          _pd_device_client,
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
			     Genode::Capability <Device_pd> pci_device_pd)
			:
				Genode::Root_component<Session_component>(ep, md_alloc),
				_pd_device_client(0),
				/* restrict physical address to 3G on 32bit with device_pd */
				_ram("dma", 0, (pci_device_pd.valid() && sizeof(void *) == 4) ?
				               0xc0000000UL : 0x100000000ULL)
			{
				_parse_config();

				if (pci_device_pd.valid())
					_pd_device_client = new (Genode::env()->heap()) Device_pd_client(pci_device_pd);

				/* associate _ram session with ram_session of process */
				_ram.ref_account(Genode::env()->ram_session_cap());
				Genode::env()->ram_session()->transfer_quota(_ram.cap(), 0x1000);

				/* enforce initial bus scan */
				bus_valid();
			}
	};

}

#endif /* _PCI_SESSION_COMPONENT_H_ */

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
#include <base/rpc_server.h>
#include <ram_session/connection.h>
#include <root/component.h>

/* os */
#include <io_mem_session/connection.h>
#include <os/config.h>
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

			Genode::Rpc_entrypoint         *_ep;
			Genode::Allocator              *_md_alloc;
			Genode::List<Device_component>  _device_list;
			Device_pd_client               *_child;
			Genode::Ram_connection         *_ram;

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

		public:

			/**
			 * Constructor
			 */
			Session_component(Genode::Rpc_entrypoint *ep,
			                  Genode::Allocator      *md_alloc,
			                  Device_pd_client       *child,
			                  Genode::Ram_connection *ram)
			:
				_ep(ep), _md_alloc(md_alloc), _child(child), _ram(ram) { }

			/**
			 * Destructor
			 */
			~Session_component()
			{
				/* release all elements of the session's device list */
				while (_device_list.first())
					release_device(_device_list.first()->cap());
			}

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

				do
				{
					function += 1;
					if (!_find_next(bus, device, function, &config, &config_access))
						return Device_capability();

					/* get new bdf values */
					bus      = config.bus_number();
					device   = config.device_number();
					function = config.function_number();
				} while ((config.class_code() ^ device_class) & class_mask);

				/* lookup if we have a extended pci config space */
				Genode::addr_t config_space = lookup_config_space(bus, device,
				                                                  function);

				/*
				 * A device was found. Create a new device component for the
				 * device and return its capability.
				 *
				 * FIXME: check and adjust session quota
				 */
				Device_component *device_component =
					new (_md_alloc) Device_component(config, config_space, _ep);

				if (!device_component)
					return Device_capability();

				_device_list.insert(device_component);
				return _ep->manage(device_component);
			}

			void release_device(Device_capability device_cap)
			{
				/* lookup device component for previous device */
				Device_component *device = dynamic_cast<Device_component *>
				                           (_ep->lookup_and_lock(device_cap));

				if (!device)
					return;

				_device_list.remove(device);
				_ep->dissolve(device);

				/* FIXME: adjust quota */
				Genode::Io_mem_connection * io_mem = device->get_config_space();
				if (io_mem)
					destroy(_md_alloc, io_mem);
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
				if (Genode::env()->ram_session()->transfer_quota(_ram->cap(),
				                                                 size))
					return Ram_capability();

				Ram_capability ram = _ram->alloc(size, Genode::UNCACHED);
				if (!ram.valid() || !_child)
					return ram;

				_child->attach_dma_mem(ram);

				return ram;
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
				/* FIXME: extract quota from args */
				/* FIXME: pass quota to session-component constructor */

				return new (md_alloc()) Session_component(ep(), md_alloc(),
				                                          _pd_device_client,
				                                          &_ram);
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
				/* restrict physical address to 4G on 32/64bit in general XXX */
				/* restrict physical address to 3G on 32bit with device_pd */
				_ram("dma", 0, (pci_device_pd.valid() && sizeof(void *) == 4) ?
				               0xc0000000UL : 0x100000000ULL)
			{
				_parse_config();

				if (pci_device_pd.valid())
					_pd_device_client = new (md_alloc) Device_pd_client(pci_device_pd);

				/* associate _ram session with ram_session of process */
				_ram.ref_account(Genode::env()->ram_session_cap());
				Genode::env()->ram_session()->transfer_quota(_ram.cap(), 0x1000);

				/* enforce initial bus scan */
				bus_valid();
			}
	};

}

#endif /* _PCI_SESSION_COMPONENT_H_ */

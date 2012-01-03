/*
 * \brief  PCI-session component
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PCI_SESSION_COMPONENT_H_
#define _PCI_SESSION_COMPONENT_H_

#include <base/rpc_server.h>
#include <pci_session/pci_session.h>
#include <root/component.h>

#include "pci_device_component.h"
#include "pci_config_access.h"

namespace Pci {

	/**
	 * Check if given PCI bus was found on inital scan
	 *
	 * This tremendously speeds up further scans by other drivers.
	 */
	bool bus_valid(int bus = 0)
	{
		struct Valid_buses
		{
			bool valid[Device_config::MAX_BUSES];

			void scan_bus(Config_access &config_access, int bus = 0)
			{
				for (int dev = 0; dev < Device_config::MAX_DEVICES; ++dev) {
					for (int fun = 0; fun < Device_config::MAX_FUNCTIONS; ++fun) {

						/* read config space */
						Device_config config(bus, dev, fun, &config_access);

						if (!config.valid())
							continue;

						/*
						 * There is at least one device on the current bus, so
						 * we mark it as valid.
						 */
						valid[bus] = true;

						/* scan behind bridge */
						if (config.is_pci_bridge()) {
							int sub_bus = config.read(&config_access,
							                          0x19, Device::ACCESS_8BIT);
							scan_bus(config_access, sub_bus);
						}
					}
				}
			}

			Valid_buses() { Config_access c; scan_bus(c); }
		};

		static Valid_buses buses;

		return buses.valid[bus];
	}

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Genode::Rpc_entrypoint         *_ep;
			Genode::Allocator              *_md_alloc;
			Genode::List<Device_component>  _device_list;


			/**
			 * Scan PCI busses for a device
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


		public:

			/**
			 * Constructor
			 */
			Session_component(Genode::Rpc_entrypoint *ep,
			                  Genode::Allocator      *md_alloc):
				_ep(ep), _md_alloc(md_alloc) { }

			/**
			 * Destructor
			 */
			~Session_component()
			{
				/* release all elements of the session's device list */
				while (_device_list.first())
					release_device(_device_list.first()->cap());
			}


			/***************************
			 ** PCI session interface **
			 ***************************/

			Device_capability first_device() {
				return next_device(Device_capability()); }

			Device_capability next_device(Device_capability prev_device)
			{
				/*
				 * Create the interface to the PCI config space.
				 * This involves the creation of I/O port sessions.
				 */
				Config_access config_access;

				/* lookup device component for previous device */
				Device_component *prev = dynamic_cast<Device_component *>
				                         (_ep->obj_by_cap(prev_device));

				/*
				 * Start bus scanning after the previous device's location.
				 * If no valid device was specified for 'prev_device', start at
				 * the beginning.
				 */
				int bus = 0, device = 0, function = 0;

				if (prev) {
					Device_config config = prev->config();
					bus      = config.bus_number();
					device   = config.device_number();
					function = config.function_number() + 1;
				}

				/*
				 * Scan busses for devices.
				 * If no device is found, return an invalid capability.
				 */
				Device_config config;
				if (!_find_next(bus, device, function, &config, &config_access))
					return Device_capability();

				/*
				 * A device was found. Create a new device component for the
				 * device and return its capability.
				 *
				 * FIXME: check and adjust session quota
				 */
				Device_component *device_component = new (_md_alloc) Device_component(config);

				if (!device_component)
					return Device_capability();

				_device_list.insert(device_component);
				return _ep->manage(device_component);
			}

			void release_device(Device_capability device_cap)
			{
				/* lookup device component for previous device */
				Device_component *device = dynamic_cast<Device_component *>
				                           (_ep->obj_by_cap(device_cap));

				if (!device)
					return;

				_device_list.remove(device);
				_ep->dissolve(device);

				/* FIXME: adjust quota */
				destroy(_md_alloc, device);
			}
	};


	class Root : public Genode::Root_component<Session_component>
	{
		private:

			Genode::Cap_session *_cap_session;

		protected:

			Session_component *_create_session(const char *args)
			{
				/* FIXME: extract quota from args */
				/* FIXME: pass quota to session-component constructor */

				return new (md_alloc()) Session_component(ep(), md_alloc());
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
			Root(Genode::Rpc_entrypoint *ep,
			     Genode::Allocator      *md_alloc)
			:
				Genode::Root_component<Session_component>(ep, md_alloc)
			{
				/* enforce initial bus scan */
				bus_valid();
			}
	};

}

#endif /* _PCI_SESSION_COMPONENT_H_ */

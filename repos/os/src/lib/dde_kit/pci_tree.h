/*
 * \brief  Virtual PCI bus tree for DDE kit
 * \author Christian Helmuth
 * \date   2008-10-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PCI_TREE_H_
#define _PCI_TREE_H_

#include <base/stdint.h>
#include <base/printf.h>
#include <base/lock.h>

#include <util/avl_tree.h>

#include <pci_session/connection.h>
#include <pci_device/client.h>

namespace Dde_kit {

	using namespace Genode;

	class Pci_device : public Avl_node<Pci_device>
	{
		public:

			static inline unsigned short knit_bdf(int bus, int dev, int fun)
			{
				return ((bus & 0xff) << 8)
				     | ((dev & 0x1f) << 3)
				     | (fun  & 0x07);
			}

		private:

			Pci::Device_client     _device;
			unsigned short         _bdf;    /* bus:device:function */

		public:

			Pci_device(Pci::Device_capability device_cap)
			:
				_device(device_cap)
			{
				unsigned char bus = ~0, dev = ~0, fun = ~0;

				_device.bus_address(&bus, &dev, &fun);
				_bdf = knit_bdf(bus, dev, fun);
			}

			/***************
			 ** Accessors **
			 ***************/

			unsigned short bdf() const { return  _bdf; }
			unsigned char  bus() const { return (_bdf >> 8) & 0xff; }
			unsigned char  dev() const { return (_bdf >> 3) & 0x1f; }
			unsigned char  fun() const { return  _bdf       & 0x07; }


			/********************************
			 ** Configuration space access **
			 ********************************/

			uint32_t config_read(unsigned char address, Pci::Device::Access_size size);

			void config_write(unsigned char address, uint32_t val,
			                  Pci::Device::Access_size size);

			/** AVL node comparison */
			bool higher(Pci_device *device) {
				return (_bdf < device->_bdf); }

			Pci_device *next(Pci_device *root, Side direction)
			{
				/*
				 * The predecessor of a node is the right-most node of the left
				 * subtree or the parent right after the first "left turn" up to
				 * the root of the tree. Symmetrically, the successor of a node is
				 * the left-most node of the right subtree or the parent right
				 * after the first "right turn" up to the root of the tree.
				 */
				if (child(direction)) {
					Pci_device *n = child(direction);
					while (true) {
						if (!n->child(!direction))
							return n;
						else
							n = n->child(!direction);
					}
				} else {
					Pci_device *n = this;
					for (Pci_device *parent = static_cast<Pci_device *>(n->_parent);
					     n != root;
					     n = parent, parent = static_cast<Pci_device *>(n->_parent)) {
						if (n == parent->child(!direction))
							return parent;
					}

					return 0;
				}
			}

			void show()
			{
				if (child(LEFT)) child(LEFT)->show();

				unsigned char ht = config_read(0x0e, Pci::Device::ACCESS_8BIT) & 0xff;

				PINF("%02x:%02x.%x %04x:%04x (%x) ht=%02x",
				     bus(), dev(), fun(),
				     _device.vendor_id(), _device.device_id(), _device.base_class(), ht);

				if (child(RIGHT)) child(RIGHT)->show();
			}

			Ram_dataspace_capability alloc_dma_buffer(Pci::Connection &pci_drv,
			                                          size_t size)
			{
				/* trigger that the device gets assigned to this driver */
				for (unsigned i = 0; i < 2; i++) {
					try {
						pci_drv.config_extended(_device);
						break;
					} catch (Pci::Device::Quota_exceeded) {
						if (i == 1)
							return Ram_dataspace_capability();
						Genode::env()->parent()->upgrade(pci_drv.cap(), "ram_quota=4096");
					}
				}

				for (unsigned i = 0; i < 2; i++) {
					try {
						return pci_drv.alloc_dma_buffer(size);
					} catch (Pci::Device::Quota_exceeded) {
						if (i == 0) {
							char buf[32];
							Genode::snprintf(buf, sizeof(buf), "ram_quota=%zd",
							                 size);
							Genode::env()->parent()->upgrade(pci_drv.cap(),
							                                 buf);
						}
					}
				}

				return Ram_dataspace_capability();
			}
	};

	class Pci_tree
	{
		public:

			class Not_found : public Exception { };

		private:

			int                  _current_dev_num;
			Pci::Connection      _pci_drv;
			Avl_tree<Pci_device> _devices;

			Lock                 _lock;

			/* Lookup device for given BDF */
			Pci_device *_lookup(unsigned short bdf)
			{
				if (!_devices.first())
					throw Not_found();

				Pci_device *d = _devices.first();

				do {
					if (bdf == d->bdf())
						return d;

					if (bdf < d->bdf())
						d = d->child(Pci_device::LEFT);
					else
						d = d->child(Pci_device::RIGHT);
				} while (d);

				throw Not_found();
			}

			/** Find lowest BDF */
			void _first_bdf(int *bus, int *dev, int *fun)
			{
				if (!_devices.first())
					throw Not_found();

				Pci_device *first, *prev, *root;
				first = prev = root = _devices.first();

				do {
					first = prev;
					prev = prev->next(root, Pci_device::LEFT);
				} while (prev);

				*bus = first->bus();
				*dev = first->dev();
				*fun = first->fun();
			}

			/** Find next BDF */
			void _next_bdf(Pci_device *prev, int *bus, int *dev, int *fun)
			{
				if (!_devices.first())
					throw Not_found();

				Pci_device *next = prev->next(_devices.first(), Pci_device::RIGHT);

				if (!next)
					throw Not_found();

				*bus = next->bus();
				*dev = next->dev();
				*fun = next->fun();
			}

			void _show_devices()
			{
				if (_devices.first())
					_devices.first()->show();
			}

		public:

			Pci_tree(unsigned device_class, unsigned class_mask);

			uint32_t config_read(int bus, int dev, int fun, unsigned char address,
			                     Pci::Device::Access_size size)
			{
				Lock::Guard lock_guard(_lock);

				unsigned short bdf = Pci_device::knit_bdf(bus, dev, fun);

				return _lookup(bdf)->config_read(address, size);
			}

			void config_write(int bus, int dev, int fun, unsigned char address,
			                  uint32_t val, Pci::Device::Access_size size)
			{
				Lock::Guard lock_guard(_lock);

				unsigned short bdf = Pci_device::knit_bdf(bus, dev, fun);

				_lookup(bdf)->config_write(address, val, size);
			}

			/**
			 * Lookup first device
			 */
			void first_device(int *bus, int *dev, int *fun)
			{
				Lock::Guard lock_guard(_lock);

				_first_bdf(bus, dev, fun);
			}

			/**
			 * Lookup next device
			 */
			void next_device(int *bus, int *dev, int *fun)
			{
				Lock::Guard lock_guard(_lock);

				Pci_device *d = _lookup(Pci_device::knit_bdf(*bus, *dev, *fun));

				_next_bdf(d, bus, dev, fun);
			}

			Ram_dataspace_capability alloc_dma_buffer(int bus, int dev,
			                                          int fun, size_t size)
			{
				Lock::Guard lock_guard(_lock);

				unsigned short bdf = Pci_device::knit_bdf(bus, dev, fun);

				return _lookup(bdf)->alloc_dma_buffer(_pci_drv, size);
			}
	};
}

#endif /* _PCI_TREE_H_ */

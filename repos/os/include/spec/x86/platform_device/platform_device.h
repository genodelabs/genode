/*
 * \brief  PCI-device interface
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

#include <base/rpc.h>
#include <base/signal.h>
#include <base/exception.h>
#include <io_mem_session/io_mem_session.h>
#include <io_port_session/capability.h>
#include <irq_session/capability.h>
#include <ram_session/ram_session.h>

/* os includes */
#include <platform_device/device.h>


namespace Platform { struct Device; }


struct Platform::Device : Platform::Abstract_device
{
	/*********************
	 ** Exception types **
	 *********************/

	class Alloc_failed    : public Genode::Exception { };
	class Quota_exceeded  : public Alloc_failed      { };


	class Resource
	{
		private:

			unsigned _bar;   /* content of base-address register */
			unsigned _size;  /* resource size                    */

		public:

			/**
			 * Resource type, either port I/O resource or memory-mapped resource
			 */
			enum Type { IO, MEMORY, INVALID };

			/**
			 * Default constructor
			 */
			Resource() { }

			/**
			 * Constructor
			 *
			 * \param bar   content of base-address register
			 * \param size  resource size
			 *
			 * This constructor is only used by the PCI bus driver
			 * that implements the server-side of the the PCI session.
			 * If bar is set to zero, the constructed resource description
			 * represents an INVALID resource.
			 */
			Resource(unsigned bar, unsigned size)
			: _bar(bar), _size(size) { }

			/**
			 * Return base address of resource
			 */
			unsigned base() const
			{
				/*
				 * Mask out the resource-description bits of the base
				 * address register. I/O resources use the lowest 3
				 * bits, memory resources use the lowest 4 bits.
				 */
				return _bar & ((type() == IO) ? ~7 : ~15);
			}

			/**
			 * Return resource size in bytes
			 */
			unsigned size() const { return _size; }

			/**
			 * Return true if resource is prefetchable memory
			 */
			bool prefetchable() const
			{
				return type() == MEMORY && (_bar & (1 << 3));
			}

			/**
			 * Return resource type
			 */
			Type type() const
			{
				if (_bar == 0)
					return INVALID;

				return (_bar & 1) ? IO : MEMORY;
			}

			/**
			 * Return raw register content
			 */
			unsigned bar() const { return _bar; }
	};

	enum { NUM_RESOURCES = 6 };

	virtual ~Device() { }

	/**
	 * Return bus, device, and function number of the device
	 */
	virtual void bus_address(unsigned char *bus, unsigned char *dev,
	                         unsigned char *fn) = 0;

	/**
	 * Return vendor ID obtained from the PCI config space
	 */
	virtual unsigned short vendor_id() = 0;

	/**
	 * Return device ID obtained from the PCI config space
	 */
	virtual unsigned short device_id() = 0;

	/**
	 * Return device class code from the PCI config space
	 */
	virtual unsigned class_code() = 0;

	/**
	 * Query PCI-resource information
	 *
	 * \param resource_id   index of according PCI resource of the device
	 *
	 * \return              resource description
	 * \retval INVALID      the supplied resource ID is invalid
	 */
	virtual Resource resource(int resource_id) = 0;

	/**
	 * Access size for operations directly accessing the config space
	 */
	enum Access_size { ACCESS_8BIT, ACCESS_16BIT, ACCESS_32BIT };

	/**
	 * Read configuration space
	 */
	virtual unsigned config_read(unsigned char address, Access_size size) = 0;

	/**
	 * Write configuration space
	 */
	virtual void config_write(unsigned char address, unsigned value,
	                          Access_size size) = 0;

	/**
	 * Query Io_port of specified bar
	 *
	 * \param id   index of according PCI resource of the device
	 */
	virtual Genode::Io_port_session_capability io_port(Genode::uint8_t id) = 0;

	/*
	 * The base classes are defined as follows:
	 *
	 *  0x00 | legacy device
	 *  0x01 | mass-storage controller
	 *  0x02 | network controller
	 *  0x03 | display controller
	 *  0x04 | multimedia device
	 *  0x05 | memory controller
	 *  0x06 | bridge device
	 *  0x07 | simple-communication controller
	 *  0x08 | base-system peripheral
	 *  0x09 | input device
	 *  0x0a | docking station
	 *  0x0b | processor
	 *  0x0c | serial bus controller
	 *  0x0d | wireless controller
	 *  0x0e | intelligent I/O controller
	 *  0x0f | satellite-communications controller
	 *  0x10 | encryption/decryption controller
	 *  0x11 | data-acquisition and signal-processing controller
	 *  0x12 | reserved
	 *  ...  |
	 *  0xff | device does not fit in any of the defined classes
	 */

	unsigned base_class() { return  class_code() >> 16; }
	unsigned sub_class()  { return (class_code() >>  8) & 0xff; }

	/**
	 * Convenience method to translate a PCI physical BAR id to a Genode
	 * virtual one usable with the io_port and io_mem methods. The virtual id
	 * is solely valid for the specific BAR type.
	 */
	Genode::uint8_t phys_bar_to_virt(Genode::uint8_t phys_bar)
	{
		Genode::uint8_t virt_io_port = 0, virt_io_mem = 0;

		for (unsigned i = 0; i < phys_bar; i++) {
			Resource::Type type = resource(i).type();
			if (type == Resource::Type::IO)
				virt_io_port ++;
			else if (type == Resource::Type::MEMORY)
				virt_io_mem ++;
		}

		Resource::Type type = resource(phys_bar).type();
		return type == Resource::Type::IO ? virt_io_port : virt_io_mem;
	}

	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_bus_address, void, bus_address,
	           unsigned char *, unsigned char *, unsigned char *);
	GENODE_RPC(Rpc_vendor_id, unsigned short, vendor_id);
	GENODE_RPC(Rpc_device_id, unsigned short, device_id);
	GENODE_RPC(Rpc_class_code, unsigned, class_code);
	GENODE_RPC(Rpc_resource, Resource, resource, int);
	GENODE_RPC(Rpc_config_read, unsigned, config_read,
	           unsigned char, Access_size);
	GENODE_RPC_THROW(Rpc_config_write, void, config_write,
	                 GENODE_TYPE_LIST(Quota_exceeded),
	                 unsigned char, unsigned, Access_size);
	GENODE_RPC(Rpc_irq, Genode::Irq_session_capability, irq, Genode::uint8_t);
	GENODE_RPC_THROW(Rpc_io_port, Genode::Io_port_session_capability, io_port,
	                 GENODE_TYPE_LIST(Quota_exceeded),
	                 Genode::uint8_t);
	GENODE_RPC_THROW(Rpc_io_mem, Genode::Io_mem_session_capability, io_mem,
	                 GENODE_TYPE_LIST(Quota_exceeded),
	                 Genode::uint8_t, Genode::Cache_attribute,
	                 Genode::addr_t, Genode::size_t);

	GENODE_RPC_INTERFACE(Rpc_bus_address, Rpc_vendor_id, Rpc_device_id,
	                     Rpc_class_code, Rpc_resource, Rpc_config_read,
	                     Rpc_config_write, Rpc_irq, Rpc_io_port, Rpc_io_mem);
};

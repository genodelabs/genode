/*
 * \brief  Client-side VirtIO bus session interface
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <irq_session/capability.h>
#include <util/interface.h>
#include <virtio_device/virt_queue.h>

namespace Virtio { struct Device; }

struct Virtio::Device : Genode::Interface
{
	using uint8_t  = Genode::uint8_t;
	using uint16_t = Genode::uint16_t;
	using uint32_t = Genode::uint32_t;

	virtual ~Device() = default;

	/**
	 * Device status values as defined in "Virtual I/O Device (VIRTIO) Version 1.0"
	 * specification, section 2.1.
	 */
	enum Status : uint8_t
	{
		RESET       = 0,
		ACKNOWLEDGE = 1 << 0,
		DRIVER      = 1 << 1,
		DRIVER_OK   = 1 << 2,
		FEATURES_OK = 1 << 3,
		FAILED      = 1 << 7,
	};

	/**
	 * Access size for opearions performed on device config space.
	 */
	enum Access_size : uint8_t
	{
		ACCESS_8BIT,
		ACCESS_16BIT,
		ACCESS_32BIT,
	};

	/**
	 * Read device vendor id register.
	 */
	virtual uint32_t vendor_id() = 0;

	/**
	 * Read device id register.
	 */
	virtual uint32_t device_id() = 0;

	/**
	 * Read current device status field.
	 */
	virtual uint8_t get_status() = 0;

	/**
	 * Set new device status.
	 *
	 * NOTE: Setting this to Status::RESET will trigger device reset.
	 *
	 * \return true if status change was successful, false oterwise.
	 */
	virtual bool set_status(uint8_t status) = 0;

	/**
	 * Get device features register value from selected register bank.
	 * The actual meaning of each bit of the features register varies
	 * between individual device types.
	 */
	virtual uint32_t get_features(uint32_t selection) = 0;

	/**
	 * Set the list of features supported by the driver to specified
	 * register bank. This must always be a subset of features read via
	 * \ref get_features call.
	 */
	virtual void set_features(uint32_t selection, uint32_t features) = 0;

	/**
	 * Read one field of device configuration space.
	 *
	 * \param offset field offset within device specific configuration space.
	 * \param size the size of the read
	 *
	 * \return value read from device configuration space.
	 */
	virtual uint32_t read_config(uint8_t offset, Access_size size) = 0;

	/**
	 * Write data into one field of device configuration space.
	 *
	 * \param offset field offset within device specific configuration space.
	 * \param size the size of the write
	 * \param value data to write.
	 */
	virtual void write_config(uint8_t offset, Access_size size, uint32_t value) = 0;

	/**
	 * Read current generation of config obtained via \ref read_config.
	 */
	virtual uint8_t get_config_generation() = 0;

	/**
	 * Read maximum allowed number of elements in the VirtIO queue identified by
	 * a given index.
	 *
	 * \return queue size in number of elements, or 0 if the queue is not available.
	 */
	virtual uint16_t get_max_queue_size(uint16_t queue_index) = 0;

	/**
	 * Configure VirtIO queue at given index using provided config structure.
	 *
	 * \param queue_index Index of the VirtIO queue to configure.
	 * \param desc VirtIO queue description.
	 *
	 * \return true if configuration was applied successfuly, false oterwise.
	 */
	virtual bool configure_queue(uint16_t queue_index, Queue_description desc) = 0;

	/**
	 * Obtain IRQ session capability for this device.
	 */
	virtual	Genode::Irq_session_capability irq() = 0;

	/**
	 * Read device interrupt status reagister. This function should also automatically
	 * clear pending IRQ bits.
	 */
	virtual uint32_t read_isr() = 0;

	/**
	 * Notify the device about new buffers being available in the specified queue.
	 */
	virtual void notify_buffers_available(uint16_t queue_index) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_vendor_id, uint32_t, vendor_id);
	GENODE_RPC(Rpc_device_id, uint32_t, device_id);
	GENODE_RPC(Rpc_get_status, uint8_t, get_status);
	GENODE_RPC(Rpc_set_status, bool, set_status, uint8_t);
	GENODE_RPC(Rpc_get_features, uint32_t, get_features, uint32_t);
	GENODE_RPC(Rpc_set_features, void, set_features, uint32_t, uint32_t);
	GENODE_RPC(Rpc_read_config, uint32_t, read_config, uint8_t, Access_size);
	GENODE_RPC(Rpc_write_config, void, write_config, uint8_t, Access_size, uint32_t);
	GENODE_RPC(Rpc_get_config_generation, uint8_t, get_config_generation);
	GENODE_RPC(Rpc_get_max_queue_size, uint16_t, get_max_queue_size, uint16_t);
	GENODE_RPC(Rpc_configure_queue, bool, configure_queue, uint16_t, Queue_description);
	GENODE_RPC(Rpc_irq, Genode::Irq_session_capability, irq);
	GENODE_RPC(Rpc_read_isr, uint32_t, read_isr);
	GENODE_RPC(Rpc_notify_buffers_available, void, notify_buffers_available, uint16_t);

	GENODE_RPC_INTERFACE(Rpc_vendor_id, Rpc_device_id,
	                     Rpc_get_status, Rpc_set_status,
	                     Rpc_get_features, Rpc_set_features,
	                     Rpc_read_config, Rpc_write_config,
	                     Rpc_get_config_generation,
	                     Rpc_get_max_queue_size, Rpc_configure_queue,
	                     Rpc_irq, Rpc_read_isr,
	                     Rpc_notify_buffers_available);
};

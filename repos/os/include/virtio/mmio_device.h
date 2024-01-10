/*
 * \brief  VirtIO MMIO device
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VIRTIO__MMIO_DEVICE_H_
#define _INCLUDE__VIRTIO__MMIO_DEVICE_H_

#include <platform_session/connection.h>
#include <platform_session/device.h>
#include <virtio/queue.h>


namespace Virtio {
	using namespace Genode;
	class Device;
}


class Virtio::Device : Platform::Device::Mmio<0x200>
{
	public:

		struct Invalid_device : Genode::Exception { };

		enum Status : uint8_t
		{
			RESET       = 0,
			ACKNOWLEDGE = 1 << 0,
			DRIVER      = 1 << 1,
			DRIVER_OK   = 1 << 2,
			FEATURES_OK = 1 << 3,
			FAILED      = 1 << 7,
		};

		enum Access_size : uint8_t
		{
			ACCESS_8BIT,
			ACCESS_16BIT,
			ACCESS_32BIT,
		};

	private:

		enum { VIRTIO_MMIO_MAGIC = 0x74726976 };

		/**
		 * Some of the registers are actually 8 bits wide,  but according to
		 * section 4.2.2.2 of VIRTIO 1.0 spec "The driver MUST use only 32 bit
		 * wide and aligned reads and writes".
		 */
		struct Magic             : Register<0x000, 32> { };
		struct Version           : Register<0x004, 32> { };
		struct DeviceID          : Register<0x008, 32> { };
		struct VendorID          : Register<0x00C, 32> { };
		struct DeviceFeatures    : Register<0x010, 32> { };
		struct DeviceFeaturesSel : Register<0x014, 32> { };
		struct DriverFeatures    : Register<0x020, 32> { };
		struct DriverFeaturesSel : Register<0x024, 32> { };
		struct QueueSel          : Register<0x030, 32> { };
		struct QueueNumMax       : Register<0x034, 32> { };
		struct QueueNum          : Register<0x038, 32> { };
		struct QueueReady        : Register<0x044, 32> { };
		struct QueueNotify       : Register<0x050, 32> { };
		struct InterruptStatus   : Register<0x060, 32> { };
		struct InterruptAck      : Register<0x064, 32> { };
		struct StatusReg         : Register<0x070, 32> { };
		struct QueueDescLow      : Register<0x080, 32> { };
		struct QueueDescHigh     : Register<0x084, 32> { };
		struct QueueAvailLow     : Register<0x090, 32> { };
		struct QueueAvailHigh    : Register<0x094, 32> { };
		struct QueueUsedLow      : Register<0x0A0, 32> { };
		struct QueueUsedHigh     : Register<0x0A4, 32> { };
		struct ConfigGeneration  : Register<0x0FC, 32> { };

		/**
		 * Different views on device configuration space. According to the
		 * VIRTIO 1.0 spec 64 bit wide registers are supposed to be read as
		 * two 32 bit values.
		 */
		template <typename T> class Config :
			public Register_array<0x100, sizeof(T)*8,
			                      256/sizeof(T), sizeof(T)*8> {};

		/*
		 * Noncopyable
		 */
		Device(Device const &) = delete;
		Device &operator = (Device const &) = delete;

		Platform::Device::Irq _irq;

	public:

		Device(Platform::Device & device)
		:
			Platform::Device::Mmio<SIZE>(device), _irq(device, {0})
		{
			if (read<Magic>() != VIRTIO_MMIO_MAGIC) {
				throw Invalid_device(); }
		}

		uint32_t vendor_id() { return read<VendorID>(); }
		uint32_t device_id() { return read<DeviceID>(); }
		uint8_t get_status() { return read<StatusReg>() & 0xff; }

		bool set_status(uint8_t status)
		{
			write<StatusReg>(status);
			return read<StatusReg>() == status;
		}

		uint32_t get_features(uint32_t selection)
		{
			write<DeviceFeaturesSel>(selection);
			return read<DeviceFeatures>();
		}

		void set_features(uint32_t selection, uint32_t features)
		{
			write<DriverFeaturesSel>(selection);
			write<DriverFeatures>(features);
		}

		uint8_t get_config_generation() {
			return read<ConfigGeneration>() & 0xff; }

		uint16_t get_max_queue_size(uint16_t queue_index)
		{
			write<QueueSel>(queue_index);
			if (read<QueueReady>() != 0) {
				return 0; }
			return (uint16_t)read<QueueNumMax>();
		}

		template <typename T>
		T read_config(const uint8_t offset)
		{
			static_assert(sizeof(T) <= 4);
			return read<Config<T>>(offset >> log2(sizeof(T)));
		}

		template <typename T>
		void write_config(const uint8_t offset, const T value)
		{
			static_assert(sizeof(T) <= 4);
			write<Config<T>>(value, (offset >> log2(sizeof(T))));
		}

		bool configure_queue(uint16_t queue_index,
		                     Virtio::Queue_description desc)
		{
			write<QueueSel>(queue_index);

			if (read<QueueReady>() != 0)
				return false;

			write<QueueNum>(desc.size);

			uint64_t addr = desc.desc;
			write<QueueDescLow>((uint32_t)addr);
			write<QueueDescHigh>((uint32_t)(addr >> 32));

			addr = desc.avail;
			write<QueueAvailLow>((uint32_t)addr);
			write<QueueAvailHigh>((uint32_t)(addr >> 32));

			addr = desc.used;
			write<QueueUsedLow>((uint32_t)addr);
			write<QueueUsedHigh>((uint32_t)(addr >> 32));

			write<QueueReady>(1);
			return read<QueueReady>() != 0;
		}

		void notify_buffers_available(uint16_t queue_index) {
			write<QueueNotify>(queue_index); }

		uint32_t read_isr()
		{
			uint32_t isr = read<InterruptStatus>();
			write<InterruptAck>(isr);
			return isr;
		}

		void irq_sigh(Signal_context_capability cap) {
			_irq.sigh(cap); }

		void irq_ack() { _irq.ack(); }
};

#endif /* _INCLUDE__VIRTIO__MMIO_DEVICE_H_ */

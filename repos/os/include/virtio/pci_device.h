/*
 * \brief  VirtIO PCI device
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VIRTIO__PCI_DEVICE_H_
#define _INCLUDE__VIRTIO__PCI_DEVICE_H_

#include <os/attached_mmio.h>
#include <irq_session/client.h>
#include <platform_device/client.h>
#include <virtio/queue.h>

namespace Virtio {
	using namespace Genode;
	struct Device_mmio;
	class Device;
}

struct Virtio::Device_mmio : public Attached_mmio
{
	struct DeviceFeatureSelect : Register<0x00, 32> { };
	struct DeviceFeature       : Register<0x04, 32> { };
	struct DriverFeatureSelect : Register<0x08, 32> { };
	struct DriverFeature       : Register<0x0C, 32> { };
	struct MsiXConfig          : Register<0x10, 16> { };
	struct NumQueues           : Register<0x12, 16> { };
	struct DeviceStatus        : Register<0x14,  8> { };
	struct ConfigGeneration    : Register<0x15,  8> { };
	struct QueueSelect         : Register<0x16, 16> { };
	struct QueueSize           : Register<0x18, 16> { };
	struct QueueMsixVector     : Register<0x1A, 16> { };
	struct QueueEnable         : Register<0x1C, 16> { };
	struct QueueNotifyOff      : Register<0x1E, 16> { };
	struct QueueDescLow        : Register<0x20, 32> { };
	struct QueueDescHigh       : Register<0x24, 32> { };
	struct QueueAvailLow       : Register<0x28, 32> { };
	struct QueueAvailHigh      : Register<0x2C, 32> { };
	struct QueueUsedLow        : Register<0x30, 32> { };
	struct QueueUsedHigh       : Register<0x34, 32> { };

	struct Config_8            : Register_array<0x0,  8, 256, 8>  { };
	struct Config_16           : Register_array<0x0, 16, 128, 16> { };
	struct Config_32           : Register_array<0x0, 32,  64, 32> { };

	struct IrqReason           : Register<0x0, 32> { };

	Device_mmio(Genode::Env    &env,
	            Genode::addr_t  base,
	            Genode::size_t  size)
	: Attached_mmio(env, base, size, false) { }
};

class Virtio::Device
{
	public:

		struct Configuration_failed : Genode::Exception { };

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

		/*
		 * Noncopyable
		 */
		Device(Device const &) = delete;
		Device &operator = (Device const &) = delete;

		enum {
			VIRTIO_PCI_BASE_ID   = 0x1040,
			VIRTIO_MSI_NO_VECTOR = 0xffff
		};

		Genode::Env                        &_env;
		Platform::Device_client            &_device;
		Genode::Irq_session_client          _irq { _device.irq(0) };
		uint32_t                            _notify_offset_multiplier = 0;
		Genode::Constructible<Device_mmio>  _cfg_common { };
		Genode::Constructible<Device_mmio>  _dev_config { };
		Genode::Constructible<Device_mmio>  _notify { };
		Genode::Constructible<Device_mmio>  _isr { };


		void _configure()
		{
			typedef Platform::Device Pdev;

			enum { PCI_STATUS = 0x6, PCI_CAPABILITIES = 0x34, };

			auto status = _device.config_read(PCI_STATUS, Pdev::ACCESS_16BIT);
			if (!(status & 0x10)) {
			    error("PCI capabilities missing according to device status!");
			    throw Configuration_failed();
			}

			auto addr = _device.config_read(PCI_CAPABILITIES, Pdev::ACCESS_8BIT);
			addr &= 0xFC;

			while (addr) {
				enum { ID_VNDR = 0x09 };
				enum { CAP_ID = 0, CAP_LIST_NEXT = 1 };

				auto const cap_id = _device.config_read(addr + CAP_ID, Pdev::ACCESS_8BIT);
				auto const cap_next = _device.config_read(addr + CAP_LIST_NEXT, Pdev::ACCESS_8BIT);

				if (cap_id == ID_VNDR) {
					enum { CFG_TYPE = 0x3, BAR = 0x4, OFFSET = 0x8,
					       LENGTH = 0xC, NOTIFY_OFFSET_MULT = 0x10 };
					enum { COMMON_CFG = 1, NOTIFY_CFG = 2, ISR_CFG = 3,
					       DEVICE_CFG = 4, PCI_CFG = 5 };

					auto const cfg_type = _device.config_read(addr + CFG_TYPE, Pdev::ACCESS_8BIT);
					auto const bar = _device.config_read(addr + BAR, Pdev::ACCESS_8BIT);
					auto const off = _device.config_read(addr + OFFSET, Pdev::ACCESS_32BIT);
					auto const len = _device.config_read(addr + LENGTH, Pdev::ACCESS_32BIT);

					if (cfg_type == COMMON_CFG) {
						auto const r = _device.resource(bar);
						_cfg_common.construct(_env, r.base() + off, len);
					} else if (cfg_type == DEVICE_CFG) {
						auto const r = _device.resource(bar);
						_dev_config.construct(_env, r.base() + off, len);
					} else if (cfg_type == NOTIFY_CFG) {
						_notify_offset_multiplier = _device.config_read(
							addr + NOTIFY_OFFSET_MULT, Pdev::ACCESS_32BIT);
						auto const r = _device.resource(bar);
						_notify.construct(_env, r.base() + off, len);
					} else if (cfg_type == ISR_CFG) {
						auto const r = _device.resource(bar);
						_isr.construct(_env, r.base() + off, len);
					}
				}

				addr = cap_next;
			}

			if (!_cfg_common.constructed() || !_dev_config.constructed() ||
			    !_notify.constructed() || !_isr.constructed()) {
				error("Required VirtIO PCI capabilities not found!");
				throw Configuration_failed();
			}

			_cfg_common->write<Device_mmio::MsiXConfig>(VIRTIO_MSI_NO_VECTOR);
		}

	public:

		Device(Genode::Env                 &env,
		       Platform::Device_client      device)
		: _env(env), _device(device)
		{
			_configure();
		}

		uint32_t vendor_id() { return _device.vendor_id(); }
		uint32_t device_id() {
			return _device.device_id() - VIRTIO_PCI_BASE_ID; }

		uint8_t get_status() {
			return _cfg_common->read<Device_mmio::DeviceStatus>(); }

		bool set_status(uint8_t status)
		{
			_cfg_common->write<Device_mmio::DeviceStatus>(status);
			return _cfg_common->read<Device_mmio::DeviceStatus>() == status;
		}

		uint32_t get_features(uint32_t selection)
		{
			_cfg_common->write<Device_mmio::DeviceFeatureSelect>(selection);
			return _cfg_common->read<Device_mmio::DeviceFeature>();
		}

		void set_features(uint32_t selection, uint32_t features)
		{
			_cfg_common->write<Device_mmio::DriverFeatureSelect>(selection);
			_cfg_common->write<Device_mmio::DriverFeature>(features);
		}

		uint8_t get_config_generation() {
			return _cfg_common->read<Device_mmio::ConfigGeneration>(); }

		uint16_t get_max_queue_size(uint16_t queue_index)
		{
			_cfg_common->write<Device_mmio::QueueSelect>(queue_index);
			return _cfg_common->read<Device_mmio::QueueSize>();
		}

		uint32_t read_config(uint8_t offset, Access_size size)
		{
			switch (size) {
			case Device::ACCESS_8BIT:
				return _dev_config->read<Device_mmio::Config_8>(offset);
			case Device::ACCESS_16BIT:
				return _dev_config->read<Device_mmio::Config_16>(offset >> 1);
			case Device::ACCESS_32BIT:
				return _dev_config->read<Device_mmio::Config_32>(offset >> 2);
			}
			return 0;
		}

		void write_config(uint8_t offset, Access_size size, uint32_t value)
		{
			switch (size) {
			case Device::ACCESS_8BIT:
				_dev_config->write<Device_mmio::Config_8>(value, offset);
				break;
			case Device::ACCESS_16BIT:
				_dev_config->write<Device_mmio::Config_16>(value, offset >> 1);
				break;
			case Device::ACCESS_32BIT:
				_dev_config->write<Device_mmio::Config_32>(value, offset >> 2);
				break;
			}
		}

		bool configure_queue(uint16_t queue_index, Virtio::Queue_description desc)
		{
			_cfg_common->write<Device_mmio::QueueSelect>(queue_index);

			if (_cfg_common->read<Device_mmio::QueueEnable>()) {
				warning("VirtIO queues can't be re-configured after being enabled!");
				return false;
			}

			_cfg_common->write<Device_mmio::QueueMsixVector>(VIRTIO_MSI_NO_VECTOR);
			if (_cfg_common->read<Device_mmio::QueueMsixVector>() != VIRTIO_MSI_NO_VECTOR) {
				error("Failed to disable MSI-X for queue ", queue_index);
				return false;
			}

			_cfg_common->write<Device_mmio::QueueSize>(desc.size);

			uint64_t addr = desc.desc;
			_cfg_common->write<Device_mmio::QueueDescLow>((uint32_t)addr);
			_cfg_common->write<Device_mmio::QueueDescHigh>((uint32_t)(addr >> 32));

			addr = desc.avail;
			_cfg_common->write<Device_mmio::QueueAvailLow>((uint32_t)addr);
			_cfg_common->write<Device_mmio::QueueAvailHigh>((uint32_t)(addr >> 32));

			addr = desc.used;
			_cfg_common->write<Device_mmio::QueueUsedLow>((uint32_t)addr);
			_cfg_common->write<Device_mmio::QueueUsedHigh>((uint32_t)(addr >> 32));
			_cfg_common->write<Device_mmio::QueueEnable>(1);
			return _cfg_common->read<Device_mmio::QueueEnable>() != 0;
		}

		void notify_buffers_available(uint16_t queue_index) {
			_cfg_common->write<Device_mmio::QueueSelect>(queue_index);
			auto const offset = _cfg_common->read<Device_mmio::QueueNotifyOff>();
			auto const addr = (offset * _notify_offset_multiplier >> 1) + 1;
			_notify->local_addr<uint16_t>()[addr] = queue_index;
		}

		uint32_t read_isr() {
			return _isr->read<Device_mmio::IrqReason>(); }

		void irq_sigh(Signal_context_capability cap) {
			_irq.sigh(cap); }

		void irq_ack() { _irq.ack_irq(); }
};

#endif /* _INCLUDE__VIRTIO__PCI_DEVICE_H_ */

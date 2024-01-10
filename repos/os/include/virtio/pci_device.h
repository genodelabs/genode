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

#include <platform_session/device.h>
#include <virtio/queue.h>

namespace Virtio {
	using namespace Genode;
	struct Device_mmio;
	class Device;
}

struct Virtio::Device_mmio : public Genode::Mmio<0x100>
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

	template <typename T> class Config :
		public Register_array<0x0, sizeof(T)*8,
		                      256/sizeof(T), sizeof(T)*8> {};

	struct IrqReason : Register<0x0, 32> { };

	using Mmio::Mmio;
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
			VIRTIO_MSI_NO_VECTOR = 0xffff,
			MMIO_MAX             = 6U
		};

		Env                  & _env;
		Platform::Connection & _plat;
		Platform::Device       _device { _plat };
		Platform::Device::Irq  _irq    { _device, { 0 } };

		Constructible<Platform::Device::Mmio<0> > _mmio[MMIO_MAX] { };

		Device_mmio _cfg_common { _bar_range("common")     };
		Device_mmio _dev_config { _bar_range("device")     };
		Device_mmio _notify     { _bar_range("notify")     };
		Device_mmio _isr        { _bar_range("irq_status") };
		size_t      _notify_offset_multiplier { 0 };

		template <typename FN>
		void with_virtio_range(String<16> type, FN const & fn)
		{
			_plat.update();
			_plat.with_xml([&] (Xml_node xml) {
				xml.with_optional_sub_node("device", [&] (Xml_node xml) {
					xml.with_optional_sub_node("pci-config",
					                           [&] (Xml_node xml) {
						xml.for_each_sub_node("virtio_range",
						                      [&] (Xml_node xml) {
							if (xml.attribute_value("type", String<16>()) ==
							    type)
								fn(xml);
						});
					});
				});
			});
		}

		Byte_range_ptr _bar_range(String<16> type)
		{
			unsigned idx = MMIO_MAX;
			addr_t   off = ~0UL;
			with_virtio_range(type, [&] (Xml_node xml) {
				idx = xml.attribute_value<unsigned>("index", MMIO_MAX);
				off = xml.attribute_value("offset", ~0UL);
			});

			if (idx >= MMIO_MAX || off == ~0UL)
				throw Configuration_failed();

			if (!_mmio[idx].constructed())
				_mmio[idx].construct(_device,
				                     Platform::Device::Mmio<0>::Index{idx});
			return _mmio[idx]->range_at(off);
		}

	public:

		Device(Genode::Env          & env,
		       Platform::Connection & plat)
		: _env(env), _plat(plat)
		{
			with_virtio_range("notify", [&] (Xml_node xml) {
				_notify_offset_multiplier = xml.attribute_value("factor", 0UL);
			});

			_cfg_common.write<Device_mmio::MsiXConfig>(VIRTIO_MSI_NO_VECTOR);
		}

		uint8_t get_status() {
			return _cfg_common.read<Device_mmio::DeviceStatus>(); }

		bool set_status(uint8_t status)
		{
			_cfg_common.write<Device_mmio::DeviceStatus>(status);
			return _cfg_common.read<Device_mmio::DeviceStatus>() == status;
		}

		uint32_t get_features(uint32_t selection)
		{
			_cfg_common.write<Device_mmio::DeviceFeatureSelect>(selection);
			return _cfg_common.read<Device_mmio::DeviceFeature>();
		}

		void set_features(uint32_t selection, uint32_t features)
		{
			_cfg_common.write<Device_mmio::DriverFeatureSelect>(selection);
			_cfg_common.write<Device_mmio::DriverFeature>(features);
		}

		uint8_t get_config_generation() {
			return _cfg_common.read<Device_mmio::ConfigGeneration>(); }

		uint16_t get_max_queue_size(uint16_t queue_index)
		{
			_cfg_common.write<Device_mmio::QueueSelect>(queue_index);
			return _cfg_common.read<Device_mmio::QueueSize>();
		}

		template <typename T>
		T read_config(const uint8_t offset)
		{
			static_assert(sizeof(T) <= 4);
			return _dev_config.read<Device_mmio::Config<T>>(offset >> log2(sizeof(T)));
		}

		template <typename T>
		void write_config(const uint8_t offset, const T value)
		{
			static_assert(sizeof(T) <= 4);
			_dev_config.write<Device_mmio::Config<T>>(value,
			                  (offset >> log2(sizeof(T))));
		}

		bool configure_queue(uint16_t queue_index, Virtio::Queue_description desc)
		{
			_cfg_common.write<Device_mmio::QueueSelect>(queue_index);

			if (_cfg_common.read<Device_mmio::QueueEnable>()) {
				warning("VirtIO queues can't be re-configured after being enabled!");
				return false;
			}

			_cfg_common.write<Device_mmio::QueueMsixVector>(VIRTIO_MSI_NO_VECTOR);
			if (_cfg_common.read<Device_mmio::QueueMsixVector>() != VIRTIO_MSI_NO_VECTOR) {
				error("Failed to disable MSI-X for queue ", queue_index);
				return false;
			}

			_cfg_common.write<Device_mmio::QueueSize>(desc.size);

			uint64_t addr = desc.desc;
			_cfg_common.write<Device_mmio::QueueDescLow>((uint32_t)addr);
			_cfg_common.write<Device_mmio::QueueDescHigh>((uint32_t)(addr >> 32));

			addr = desc.avail;
			_cfg_common.write<Device_mmio::QueueAvailLow>((uint32_t)addr);
			_cfg_common.write<Device_mmio::QueueAvailHigh>((uint32_t)(addr >> 32));

			addr = desc.used;
			_cfg_common.write<Device_mmio::QueueUsedLow>((uint32_t)addr);
			_cfg_common.write<Device_mmio::QueueUsedHigh>((uint32_t)(addr >> 32));
			_cfg_common.write<Device_mmio::QueueEnable>(1);
			return _cfg_common.read<Device_mmio::QueueEnable>() != 0;
		}

		void notify_buffers_available(uint16_t queue_index) {
			_cfg_common.write<Device_mmio::QueueSelect>(queue_index);
			auto const offset = _cfg_common.read<Device_mmio::QueueNotifyOff>();
			auto const addr = (offset * _notify_offset_multiplier >> 1) + 1;
			*(uint16_t*)(_notify.base() + addr) = queue_index;
		}

		uint32_t read_isr() {
			return _isr.read<Device_mmio::IrqReason>(); }

		void irq_sigh(Signal_context_capability cap) {
			_irq.sigh(cap); }

		void irq_ack() { _irq.ack(); }
};

#endif /* _INCLUDE__VIRTIO__PCI_DEVICE_H_ */

/*
 * \brief  VirtIO PCI driver
 * \author Piotr Tworek
 * \date   2020-01-22
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <virtio_device/xml_node.h>

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <io_mem_session/connection.h>
#include <os/session_policy.h>
#include <os/attached_mmio.h>
#include <platform_session/client.h>
#include <platform_session/connection.h>
#include <root/component.h>
#include <virtio_device/virtio_device.h>
#include <virtio_session/virtio_session.h>

namespace Virtio_pci
{
	using namespace Genode;
	using namespace Virtio;

	class Device_component;
	class Session_component;
	class Root;
	struct Main;
	struct Device_description;
	class Device_mmio;

	typedef Genode::List<Device_description> Device_description_list;

	enum { VIRTIO_PCI_BASE_ID = 0x1040 };

	bool is_legacy_device(unsigned short device_id) {
		return device_id < VIRTIO_PCI_BASE_ID; }

	Virtio::Device_type pci_to_virtio_device_type(unsigned short device_id) {
		if (device_id < VIRTIO_PCI_BASE_ID ||
		    device_id > VIRTIO_PCI_BASE_ID + Device_type::UNKNOWN)
			return Device_type::UNKNOWN;
		return static_cast<Device_type>(device_id - VIRTIO_PCI_BASE_ID);
	}
}


struct Virtio_pci::Device_description : Genode::List<Device_description>::Element
{
	Device_description(Virtio::Device_type t, Platform::Device_capability cap)
	    : type(t), device_cap(cap) { }

	const Virtio::Device_type   type;
	Platform::Device_capability device_cap;
	bool                        claimed = false;
};


class Virtio_pci::Device_mmio : public Attached_mmio
{
	private:
		Genode::Cap_quota_guard &_cap_guard;

	public:

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

		Device_mmio(Genode::Env             &env,
		            Genode::Cap_quota_guard &cap_guard,
		            Genode::addr_t           base,
		            Genode::size_t           size)
		: Attached_mmio(env, base, size, false), _cap_guard(cap_guard)
		{
			_cap_guard.withdraw(Genode::Cap_quota{1});
		}

		~Device_mmio() { _cap_guard.replenish(Genode::Cap_quota{1}); }
};


class Virtio_pci::Device_component : public Genode::Rpc_object<Virtio::Device>,
                                     private Genode::List<Device_component>::Element
{
	public:

		struct Configuration_failed : Exception { };

	private:

		friend class Genode::List<Device_component>;

		enum { VIRTIO_MSI_NO_VECTOR = 0xffff };

		/*
		 * Noncopyable
		 */
		Device_component(Device_component const &);
		Device_component &operator = (Device_component const &);

		Genode::Env                       &_env;
		Genode::Cap_quota_guard           &_cap_guard;
		Device_description                &_device_description;
		Platform::Device_client            _device { _device_description.device_cap };
		Genode::uint32_t                   _notify_offset_multiplier = 0;
		Genode::Constructible<Device_mmio> _cfg_common { };
		Genode::Constructible<Device_mmio> _dev_config { };
		Genode::Constructible<Device_mmio> _notify { };
		Genode::Constructible<Device_mmio> _isr { };


		/***********************************
		 ** Virtio::Device implementation **
		 ***********************************/

		uint32_t vendor_id() override { return _device.vendor_id(); }
		uint32_t device_id() override { return _device_description.type; }

		uint8_t get_status() override {
			return (Device::Status)_cfg_common->read<Device_mmio::DeviceStatus>(); }

		bool set_status(uint8_t status) override
		{
			_cfg_common->write<Device_mmio::DeviceStatus>(status);
			return _cfg_common->read<Device_mmio::DeviceStatus>() == status;
		}

		uint32_t get_features(uint32_t selection) override
		{
			_cfg_common->write<Device_mmio::DeviceFeatureSelect>(selection);
			return _cfg_common->read<Device_mmio::DeviceFeature>();
		}

		void set_features(uint32_t selection, uint32_t features) override
		{
			_cfg_common->write<Device_mmio::DriverFeatureSelect>(selection);
			_cfg_common->write<Device_mmio::DriverFeature>(features);
		}

		uint32_t read_config(uint8_t offset, Access_size size) override
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

		void write_config(uint8_t offset, Access_size size, uint32_t value) override
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

		uint8_t get_config_generation() override {
			return _cfg_common->read<Device_mmio::ConfigGeneration>(); }

		uint16_t get_max_queue_size(uint16_t queue_index) override
		{
			_cfg_common->write<Device_mmio::QueueSelect>(queue_index);
			return _cfg_common->read<Device_mmio::QueueSize>();
		}

		bool configure_queue(uint16_t queue_index, Virtio::Queue_description desc) override
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

		Genode::Irq_session_capability irq() override { return _device.irq(0); }

		uint32_t read_isr() override {
			return _isr->read<Device_mmio::IrqReason>(); }

		void notify_buffers_available(uint16_t queue_index) override
		{
			_cfg_common->write<Device_mmio::QueueSelect>(queue_index);
			auto const offset = _cfg_common->read<Device_mmio::QueueNotifyOff>();
			auto const addr = (offset * _notify_offset_multiplier >> 1) + 1;
			_notify->local_addr<uint16_t>()[addr] = queue_index;
		}

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
					enum { CFG_TYPE = 0x3, BAR = 0x4, OFFSET = 0x8, LENGTH = 0xC, NOTIFY_OFFSET_MULT = 0x10 };
					enum { COMMON_CFG = 1, NOTIFY_CFG = 2, ISR_CFG = 3, DEVICE_CFG = 4, PCI_CFG = 5 };

					auto const cfg_type = _device.config_read(addr + CFG_TYPE, Pdev::ACCESS_8BIT);
					auto const bar = _device.config_read(addr + BAR, Pdev::ACCESS_8BIT);
					auto const off = _device.config_read(addr + OFFSET, Pdev::ACCESS_32BIT);
					auto const len = _device.config_read(addr + LENGTH, Pdev::ACCESS_32BIT);

					if (cfg_type == COMMON_CFG) {
						auto const r = _device.resource(bar);
						_cfg_common.construct(_env, _cap_guard, r.base() + off, len);
					} else if (cfg_type == DEVICE_CFG) {
						auto const r = _device.resource(bar);
						_dev_config.construct(_env, _cap_guard, r.base() + off, len);
					} else if (cfg_type == NOTIFY_CFG) {
						_notify_offset_multiplier = _device.config_read(
							addr + NOTIFY_OFFSET_MULT, Pdev::ACCESS_32BIT);
						auto const r = _device.resource(bar);
						_notify.construct(_env, _cap_guard, r.base() + off, len);
					} else if (cfg_type == ISR_CFG) {
						auto const r = _device.resource(bar);
						_isr.construct(_env, _cap_guard, r.base() + off, len);
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

		const Device_description &description() const {
			return _device_description; }

		Device_component(Genode::Env             &env,
                         Genode::Cap_quota_guard &cap_guard,
		                 Device_description      &description)
		: _env(env),
		  _cap_guard(cap_guard),
		  _device_description(description)
		{
			/* RPC ep + Device_client cap + PCI device IRQ session */
			_cap_guard.withdraw(Genode::Cap_quota{3});
			_device_description.claimed = true;
			_configure();
		}

		~Device_component()
		{
			_cap_guard.replenish(Genode::Cap_quota{3});
			_device_description.claimed = false;
		}
};


class Virtio_pci::Session_component : public Rpc_object<Virtio::Session>
{
	private:

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

		enum {
			SLAB_BLOCK_SIZE = (sizeof(Device_component) + 32) * DEVICE_SLOT_COUNT,
		};

		Genode::Env                                     &_env;
		Genode::Attached_rom_dataspace                  &_config;
		Device_description_list                         &_device_description_list;
		Genode::Cap_quota_guard                          _cap_guard;
		Genode::Session_label                      const _label;
		Genode::Session_policy                     const _policy { _label, _config.xml() };
		Genode::uint8_t                                  _slab_data[SLAB_BLOCK_SIZE];
		Genode::Tslab<Device_component, SLAB_BLOCK_SIZE> _slab;
		Genode::List<Device_component>                   _device_list { };

		/**
		 * Check according session policy device usage
		 */
		bool _permit_device(Device_type type)
		{
			try {
				_policy.for_each_sub_node("device", [&] (Xml_node node) {
					if (!node.has_attribute("type"))
						return;

					Device_type type_from_policy =
						node.attribute_value("type", Device_type::INVALID);

					if (type == type_from_policy)
						throw true;
				});
			} catch (bool result) { return result; }

			return false;
		}

		Device_capability _find_device(Device_component* prev_device, Device_type type)
		{
			Device_description *desc = nullptr;

			if (!_permit_device(type))
				return Device_capability();

			if (prev_device) {
				desc = prev_device->description().next();
			} else {
				desc = _device_description_list.first();
			}

			while (desc) {
				if (!desc->claimed && desc->type == type)
					break;
				desc = desc->next();
			}

			if (!desc)
				return Device_capability();

			try {
				auto *device = new (_slab) Device_component(_env, _cap_guard, *desc);
				_device_list.insert(device);
				return _env.ep().rpc_ep().manage(device);
			} catch (Genode::Out_of_ram) {
				warning("'", _label, "' - too many claimed devices!");
				throw Out_of_device_slots();
			} catch (Genode::Out_of_caps) {
				warning("'", _label, "' - Out_of_caps during Device_component construction!");
				throw;
			} catch (...) {
				warning("'", _label, "' - failed to create Device_component!");
				return Device_capability();
			}
		}

	public:

		Session_component(Genode::Env                      &env,
		                  Genode::Attached_rom_dataspace   &config,
		                  Device_description_list          &device_list,
		                  char                       const *args)
		: _env(env),
		  _config(config),
		  _device_description_list(device_list),
		  _cap_guard(Genode::cap_quota_from_args(args)),
		  _label(Genode::label_from_args(args)),
		  _slab(nullptr, _slab_data) {
		}

		~Session_component()
		{
			while (_device_list.first())
				release_device(static_cast<Rpc_object<Virtio::Device> *>(
					_device_list.first())->cap());
		}

		void upgrade_resources(Genode::Session::Resources resources) {
			_cap_guard.upgrade(resources.cap_quota); }

		Device_capability first_device(Device_type type) override
		{
			auto lambda = [&] (Device_component *prev) {
				return _find_device(prev, type); };
			return _env.ep().rpc_ep().apply(Device_capability(), lambda);
		}

		Device_capability next_device(Device_capability prev_device) override
		{
			auto lambda = [&] (Device_component *prev) {
				if (!prev)
					return Device_capability();
				return _find_device(prev, prev->description().type);
			};
			return _env.ep().rpc_ep().apply(prev_device, lambda);
		}

		void release_device(Device_capability device_cap) override
		{
			Device_component *device;
			auto lambda = [&] (Device_component *d) {
				device = d;
				if (!device)
					return;

				_device_list.remove(device);
				_env.ep().rpc_ep().dissolve(device);
			};

			_env.ep().rpc_ep().apply(device_cap, lambda);

			if (device)
				destroy(_slab, device);
		}
};


class Virtio_pci::Root : public Root_component<Session_component>
{
	private:

		/*
		 * Noncopyable
		 */
		Root(Root const &);
		Root &operator = (Root const &);

		Genode::Env                    &_env;
		Genode::Attached_rom_dataspace  _config_rom { _env, "config" };
		Genode::Heap                    _local_heap { _env.ram(), _env.rm() };
		Platform::Connection            _pci { _env };
		Device_description_list         _device_description_list {};


		Session_component *_create_session(const char *args) override {
			return new (md_alloc()) Session_component(
				_env, _config_rom, _device_description_list, args); }

		void _upgrade_session(Session_component *s, const char *args) override {
			s->upgrade_resources(Genode::session_resources_from_args(args)); }

		void _probe()
		{
			enum { VIRTIO_VENDOR_ID = 0x1AF4 };

			Platform::Device_capability prev_device_cap, device_cap;

			size_t device_count = 0;

			_pci.with_upgrade([&] () { device_cap = _pci.first_device(); });

			while (device_cap.valid()) {
				bool claim = false;
				{
					Platform::Device_client device(device_cap);
					const unsigned short vendor_id = device.vendor_id();
					const unsigned short device_id = device.device_id();

					if (vendor_id == VIRTIO_VENDOR_ID && is_legacy_device(device_id)) {
						warning("Found usupported legacy VirtIO PCI device: ", Hex(device_id));
					} else if (vendor_id == VIRTIO_VENDOR_ID) {
						const auto type = pci_to_virtio_device_type(device_id);

						unsigned char bus = 0, dev = 0, fun = 0;
						device.bus_address(&bus, &dev, &fun);

						log("Found VirtIO ", type, " device @ PCI ",
						    Hex(bus, Hex::OMIT_PREFIX), ":",
						    Hex(dev, Hex::OMIT_PREFIX), ".",
						    Hex(fun, Hex::OMIT_PREFIX), " ");

						auto *d = new (md_alloc()) Device_description(type, device_cap);
						_device_description_list.insert(d);
						claim = true;
						device_count++;
					}
				}

				_pci.release_device(prev_device_cap);

				if (!claim) {
					prev_device_cap = device_cap;
				} else {
					prev_device_cap = Platform::Device_capability();
				}

				_pci.with_upgrade([&] () { device_cap = _pci.next_device(device_cap); });
			}
			log("Probe finished, found ", device_count, " VirtIO device(s).");
		}

	public:

		Root(Env &env, Allocator &md_alloc)
		: Root_component<Session_component>(env.ep(), md_alloc), _env(env)
		{
			_probe();
			if (!_device_description_list.first()) {
				warning("No VirtIO devices found!");
				env.parent().exit(-1);
			}
		}

        ~Root()
        {
		Device_description *d = nullptr;
		while ((d = _device_description_list.first())) {
			_pci.release_device(d->device_cap);
			_device_description_list.remove(d);
		}
        }
};


struct Virtio_pci::Main
{
	Genode::Env         &_env;
	Genode::Sliced_heap  _heap { _env.ram(), _env.rm() };
	Root                 _root { _env, _heap };

	Main(Genode::Env &env) : _env(env)
	{
		log("--- VirtIO PCI driver started ---");
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Virtio_pci::Main main(env);
}


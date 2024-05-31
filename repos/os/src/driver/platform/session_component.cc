/*
 * \brief  Platform driver - session component
 * \author Stefan Kalkowski
 * \date   2020-04-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <dataspace/client.h>
#include <base/attached_io_mem_dataspace.h>

#include <device.h>
#include <pci.h>
#include <session_component.h>

using Driver::Session_component;


Genode::Capability<Platform::Device_interface>
Session_component::_acquire(Device & device)
{
	/**
	 * add IOMMU domains if they don't exist yet
	 *
	 * note: must be done before device.acquire and Device_component construction
	 *       because both may access the domain registry
	 */
	device.for_each_io_mmu([&] (Device::Io_mmu const & io_mmu) {
		_domain_registry.with_domain(io_mmu.name,
			[&] (Io_mmu::Domain &) { },
			[&] () {
				_io_mmu_devices.for_each([&] (Io_mmu & io_mmu_dev) {
					if (io_mmu_dev.name() == io_mmu.name) {
						if (io_mmu_dev.mpu() && _dma_allocator.remapping())
							error("Unable to create domain for MPU device ",
							      io_mmu_dev.name(), " for an IOMMU-enabled session.");
						else
							new (heap()) Io_mmu_domain(_domain_registry,
							                           io_mmu_dev,
							                           heap(),
							                           _env_ram,
							                           _dma_allocator.buffer_registry(),
							                           _ram_quota_guard(),
							                           _cap_quota_guard());
					}
				});
			}
		);},

		/* empty list fn */
		[&] () { }
	);

	Device_component * dc = new (heap())
		Device_component(_device_registry, _env, *this, _devices, device);

	device.acquire(*this);
	return _env.ep().rpc_ep().manage(dc);
};


void Session_component::_release_device(Device_component & dc)
{
	Device::Name name = dc.device();
	_env.ep().rpc_ep().dissolve(&dc);
	destroy(heap(), &dc);

	_devices.for_each([&] (Device & dev) {
		if (name == dev.name()) dev.release(*this); });

	/* destroy unused domains */
	_domain_registry.for_each([&] (Io_mmu_domain & wrapper) {
		if (wrapper.domain.devices() == 0)
			destroy(heap(), &wrapper);
	});
}


void Session_component::_free_dma_buffer(Dma_buffer & buf)
{
	Ram_dataspace_capability cap = buf.cap;

	_domain_registry.for_each_domain([&] (Io_mmu::Domain & domain) {
		domain.remove_range({ buf.dma_addr, buf.size });
	});

	destroy(heap(), &buf);
	_env_ram.free(cap);
}


bool Session_component::matches(Device const & dev) const
{
	bool ret = false;

	try {
		Session_policy const policy { label(), _config.xml() };

		/* check PCI devices */
		if (pci_device_matches(policy, dev))
			return true;

		/* check for dedicated device name */
		policy.for_each_sub_node("device", [&] (Xml_node node) {
			if (dev.name() == node.attribute_value("name", Device::Name()))
				ret = true;
		});
	} catch (Session_policy::No_policy_defined) { }

	return ret;
};


void Session_component::update_io_mmu_devices()
{
	_io_mmu_devices.for_each([&] (Io_mmu & io_mmu_dev) {

		/* determine whether IOMMU is used by any owned/acquire device */
		bool used_by_owned_device = false;
		_devices.for_each([&] (Device const & dev) {
			if (!(dev.owner() == _owner_id))
				return;

			if (used_by_owned_device)
				return;

			dev.for_each_io_mmu(
				[&] (Device::Io_mmu const & io_mmu) {
					if (io_mmu.name == io_mmu_dev.name())
						used_by_owned_device = true;
				},
				[&] () { });
		});

		/* synchronise with IOMMU domains */
		bool domain_exists = false;
		_domain_registry.for_each([&] (Io_mmu_domain & wrapper) {
			if (io_mmu_dev.domain_owner(wrapper.domain)) {
				domain_exists = true;

				/* remove domain if not used by any owned device */
				if (!used_by_owned_device)
					destroy(heap(), &wrapper);
			}
		});

		/**
		 * If an IOMMU is used but there is no domain (because the IOMMU
		 * device was just added), we need to create (i.e. allocate) a domain for
		 * it. However, since we are in context of a ROM update at this point,
		 * we are not able to propagate any Out_of_ram exception to the client.
		 * Since this is supposedly a very rare and not even practical corner-case,
		 * we print a warning instead.
		 */
		if (used_by_owned_device && !domain_exists) {
			warning("Unable to configure DMA ranges properly because ",
			        "IO MMU'", io_mmu_dev.name(),
			        "' was added to an already acquired device.");
		}

	});
}


void Session_component::update_policy(bool info, Policy_version version)
{
	_info    = info;
	_version = version;

	update_io_mmu_devices();

	enum Device_state { AWAY, CHANGED, UNCHANGED };

	_device_registry.for_each([&] (Device_component & dc) {
		Device_state state = AWAY;
		_devices.for_each([&] (Device const & dev) {
			if (dev.name() != dc.device())
				return;
			state = (dev.owner() == _owner_id && matches(dev)) ? UNCHANGED : CHANGED;
		});

		if (state == UNCHANGED)
			return;

		if (state == CHANGED)
			warning("Device ", dc.device(),
			        " has changed, will close device session");
		else
			warning("Device ", dc.device(),
			        " unavailable, will close device session");
		_release_device(dc);
	});

	update_devices_rom();
};


void Session_component::produce_xml(Xml_generator &xml)
{
	if (_version.valid())
		xml.attribute("version", _version);

	_devices.for_each([&] (Device const & dev) {
		if (matches(dev)) dev.generate(xml, _info); });
}


Genode::Heap & Session_component::heap() { return _md_alloc; }


Driver::Io_mmu_domain_registry & Session_component::domain_registry()
{
	return _domain_registry;
}


Driver::Dma_allocator & Session_component::dma_allocator()
{
	return _dma_allocator;
}


void Session_component::update_devices_rom()
{
	_rom_session.trigger_update();
}


void Session_component::enable_device(Device const & device)
{
	auto fn = [&] (Driver::Io_mmu::Domain & domain) {
		device.for_pci_config([&] (Device::Pci_config const & cfg) {
			Attached_io_mem_dataspace io_mem { _env, cfg.addr, 0x1000 };
			domain.enable_pci_device(io_mem.cap(),
			                         {cfg.bus_num, cfg.dev_num, cfg.func_num});
		});
		domain.enable_device();
	};

	auto default_domain_fn = [&] () { _domain_registry.with_default_domain(fn); };

	device.for_each_io_mmu(
		/* non-empty list fn */
		[&] (Device::Io_mmu const & io_mmu) {
			_domain_registry.with_domain(io_mmu.name, fn, default_domain_fn);
		},

		/* empty list fn */
		default_domain_fn
	);

	pci_enable(_env, device);
}


void Session_component::disable_device(Device const & device)
{
	pci_disable(_env, device);

	auto fn = [&] (Driver::Io_mmu::Domain & domain) {
		device.for_pci_config([&] (Device::Pci_config const & cfg) {
			domain.disable_pci_device({cfg.bus_num, cfg.dev_num, cfg.func_num});
		});
		domain.disable_device();
	};

	auto default_domain_fn = [&] () { _domain_registry.with_default_domain(fn); };

	device.for_each_io_mmu(
		/* non-empty list fn */
		[&] (Device::Io_mmu const & io_mmu) {
			_domain_registry.with_domain(io_mmu.name, fn, default_domain_fn); },

		/* empty list fn */
		default_domain_fn
	);
}


Genode::Rom_session_capability Session_component::devices_rom() {
	return _rom_session.cap(); }


Genode::Capability<Platform::Device_interface>
Session_component::acquire_device(Platform::Session::Device_name const &name)
{
	Capability<Platform::Device_interface> cap;

	_devices.for_each([&] (Device & dev)
	{
		if (dev.name() != name || !matches(dev))
			return;
		if (dev.owner().valid())
			warning("Cannot aquire device ", name, " already in use");
		else
			cap = _acquire(dev);
	});

	return cap;
}


Genode::Capability<Platform::Device_interface>
Session_component::acquire_single_device()
{
	Capability<Platform::Device_interface> cap;

	_devices.for_each([&] (Device & dev) {
		if (!cap.valid() && matches(dev) && !dev.owner().valid())
			cap = _acquire(dev); });

	return cap;
}


void Session_component::release_device(Capability<Platform::Device_interface> device_cap)
{
	if (!device_cap.valid())
		return;

	_device_registry.for_each([&] (Device_component & dc) {
		if (device_cap.local_name() == dc.cap().local_name())
			_release_device(dc); });
}


Genode::Ram_dataspace_capability
Session_component::alloc_dma_buffer(size_t const size, Cache cache)
{
	struct Guard {

		Constrained_ram_allocator & _env_ram;
		Heap                      & _heap;
		bool                        _cleanup { true };

		Ram_dataspace_capability   ram_cap { };
		struct {
			Dma_buffer * buf     { nullptr };
		};

		void disarm() { _cleanup = false; }

		Guard(Constrained_ram_allocator & env_ram, Heap & heap)
		: _env_ram(env_ram), _heap(heap)
		{ }

		~Guard()
		{
			if (_cleanup && buf)
				destroy(_heap, buf);

			if (_cleanup && ram_cap.valid())
				_env_ram.free(ram_cap);
		}
	} guard { _env_ram, heap() };

	/*
	 * Check available quota beforehand and reflect the state back
	 * to the client because the 'Expanding_pd_session_client' will
	 * ask its parent otherwise.
	 */
	enum { WATERMARK_CAP_QUOTA = 8, };
	if (_env.pd().avail_caps().value < WATERMARK_CAP_QUOTA)
		throw Out_of_caps();

	enum { WATERMARK_RAM_QUOTA = 4096, };
	if (_env.pd().avail_ram().value < WATERMARK_RAM_QUOTA)
		throw Out_of_ram();

	try {
		guard.ram_cap = _env_ram.alloc(size, cache);
	} catch (Ram_allocator::Denied) { }

	if (!guard.ram_cap.valid()) return guard.ram_cap;


	try {
		Dma_buffer & buf = _dma_allocator.alloc_buffer(guard.ram_cap,
		                                               _env.pd().dma_addr(guard.ram_cap),
		                                               _env_ram.dataspace_size(guard.ram_cap));
		guard.buf = &buf;

		_domain_registry.for_each_domain([&] (Io_mmu::Domain & domain) {
			domain.add_range({ buf.dma_addr, buf.size }, buf.phys_addr, buf.cap);
		});
	} catch (Dma_allocator::Out_of_virtual_memory) { }

	guard.disarm();
	return guard.ram_cap;
}


void Session_component::free_dma_buffer(Ram_dataspace_capability ram_cap)
{
	if (!ram_cap.valid()) { return; }

	_dma_allocator.buffer_registry().for_each([&] (Dma_buffer & buf) {
		if (buf.cap.local_name() == ram_cap.local_name())
			_free_dma_buffer(buf); });
}


Genode::addr_t Session_component::dma_addr(Ram_dataspace_capability ram_cap)
{
	addr_t ret = 0;

	if (!ram_cap.valid())
		return ret;

	_dma_allocator.buffer_registry().for_each([&] (Dma_buffer const & buf) {
		if (buf.cap.local_name() == ram_cap.local_name())
			ret = buf.dma_addr; });

	return ret;
}


Session_component::Session_component(Env                          & env,
                                     Attached_rom_dataspace const & config,
                                     Device_model                 & devices,
                                     Session_registry             & registry,
                                     Io_mmu_devices               & io_mmu_devices,
                                     Label          const         & label,
                                     Resources      const         & resources,
                                     Diag           const         & diag,
                                     bool           const           info,
                                     Policy_version const           version,
                                     bool           const           dma_remapping,
                                     bool           const           kernel_iommu)
:
	Session_object<Platform::Session>(env.ep(), resources, label, diag),
	Session_registry::Element(registry, *this),
	Dynamic_rom_session::Xml_producer("devices"),
	_env(env), _config(config), _devices(devices),
	_io_mmu_devices(io_mmu_devices), _info(info), _version(version),
	_dma_allocator(_md_alloc, dma_remapping)
{
	/*
	 * FIXME: As the ROM session does not propagate Out_of_*
	 *        exceptions resp. does not account costs for the ROM
	 *        dataspace to the client for the moment, we cannot do
	 *        so in the dynamic rom session, and cannot use the
	 *        constrained ram allocator within it. Therefore,
	 *        we account the costs here until the ROM session interface
	 *        changes.
	 */
	_cap_quota_guard().withdraw(Cap_quota{Rom_session::CAP_QUOTA});
	_ram_quota_guard().withdraw(Ram_quota{5*1024});

	/**
	 * Until we integrated IOMMU support within the platform driver, we assume
	 * there is a kernel_iommu used by each device if _iommu is set. We therefore
	 * construct a corresponding domain object at session construction.
	 */
	if (kernel_iommu)
		_io_mmu_devices.for_each([&] (Io_mmu & io_mmu_dev) {
			if (io_mmu_dev.name() == "kernel_iommu") {
				_domain_registry.default_domain(io_mmu_dev,
				                                heap(),
				                                _env_ram,
				                                _dma_allocator.buffer_registry(),
				                                _ram_quota_guard(),
				                                _cap_quota_guard());
			}
		});
}


Session_component::~Session_component()
{
	_device_registry.for_each([&] (Device_component & dc) {
		_release_device(dc); });

	_domain_registry.for_each([&] (Io_mmu_domain & wrapper) {
		destroy(heap(), &wrapper); });

	/* free up dma buffers */
	_dma_allocator.buffer_registry().for_each([&] (Dma_buffer & buf) {
		_free_dma_buffer(buf); });

	/* replenish quota for rom sessions, see constructor for explanation */
	_cap_quota_guard().replenish(Cap_quota{Rom_session::CAP_QUOTA});
	_ram_quota_guard().replenish(Ram_quota{5*1024});
}

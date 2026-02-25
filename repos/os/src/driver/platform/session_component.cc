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

#include <device.h>
#include <pci.h>
#include <session_component.h>

using Driver::Session_component;


Genode::Capability<Platform::Device_interface>
Session_component::_acquire(Device &device)
{
	Device_component * dc = new (heap())
		Device_component(_device_registry, _env, *this, _devices, device);

	device.acquire(*this);
	update_devices_rom();

	return _env.ep().rpc_ep().manage(dc);
};


void Session_component::_release_device(Device_component &dc)
{
	_env.ep().rpc_ep().dissolve(&dc);

	/* release device (calls disable_device()) before destroying device component */
	Device::Name name = dc.device();
	_devices.for_each([&] (Device &dev) {
		if (name == dev.name()) dev.release(*this); });

	/* destroy device component */
	destroy(heap(), &dc);
	update_devices_rom();
}


void Session_component::_free_dma_buffer(Dma_buffer &buf)
{
	Ram_dataspace_capability cap = buf.cap;

	_domain.remove_range({ buf.dma_addr, buf.size });
	for_each_io_mmu([&] (auto &io_mmu) {
		io_mmu.iotlb_flush(_domain); });

	destroy(heap(), &buf);
	_env_ram.free(cap);
}


Driver::Io_mmu::Domain & Session_component::_create_domain()
{
	/* Use non-functional domain if no IOMMU is in use */
	static Io_mmu::Domain dummy { *(Allocator*)(nullptr) };

	Io_mmu::Domain *domain = &dummy;

	_devices.for_each([&] (Device const &dev) {
		if (!matches(dev) || domain != &dummy)
			return;

		_devices.with_io_mmu(dev, [&] (auto &io_mmu) {
			domain = &io_mmu.create_domain(heap(), _env_ram,
			                               _ram_quota_guard(),
			                               _cap_quota_guard());
		});
	});

	return *domain;
}


bool Session_component::_dma_remapable() const
{
	/* iterate IOMMU devices and determine address translation mode */
	bool mpu_present   { false };
	bool iommu_present { false };

	_devices.for_each([&] (Device const &dev) {
		if (!matches(dev)) return;

		_devices.with_io_mmu(dev, [&] (auto &io_mmu) {
			if (io_mmu.mpu()) mpu_present   = true;
			else              iommu_present = true;
		});
	});

	return iommu_present && !mpu_present;
}


bool Session_component::matches(Device const &dev) const
{
	return with_matching_policy(label(), _config.node(),
		[&] (Node const &policy) {

			/* check PCI devices */
			if (pci_device_matches(policy, dev))
				return true;

			/* check for dedicated device name */
			bool ret = false;
			policy.for_each_sub_node("device", [&] (Node const &node) {
				if (dev.name() == node.attribute_value("name", Device::Name()))
					ret = true;
			});
			return ret;

		}, [] { return false; });
};


void Session_component::update_policy(bool info, Policy_version version)
{
	_info    = info;
	_version = version;

	enum Device_state { AWAY, CHANGED, UNCHANGED };

	_device_registry.for_each([&] (Device_component &dc) {
		Device_state state = AWAY;
		_devices.for_each([&] (Device const &dev) {
			if (dev.name() != dc.device())
				return;
			state = (dev.owner(*this) && matches(dev)) ? UNCHANGED : CHANGED;
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


void Session_component::generate(Generator &g)
{
	if (_version.valid())
		g.attribute("version", _version);

	_devices.for_each([&] (Device const &dev) {
		if (matches(dev)) dev.generate(g, _info); });
}


Genode::Heap & Session_component::heap() { return _md_alloc; }


void Session_component::update_devices_rom()
{
	_rom_session.trigger_update();
}


void Session_component::enable_device(Device const &device)
{
	_devices.with_io_mmu(device, [&] (auto &io_mmu) {
		io_mmu.enregister(device, _domain); });
	pci_enable(_env, device);
}


void Session_component::disable_device(Device const &device)
{
	pci_disable(_env, device);
	_devices.with_io_mmu(device, [&] (auto &io_mmu) {
		io_mmu.deregister(device, _domain); });
}


Genode::Rom_session_capability Session_component::devices_rom() {
	return _rom_session.cap(); }


Genode::Capability<Platform::Device_interface>
Session_component::acquire_device(Platform::Session::Device_name const &name)
{
	Capability<Platform::Device_interface> cap;

	_devices.for_each([&] (Device &dev)
	{
		if (dev.name() != name || !matches(dev))
			return;
		if (dev.owned())
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

	_devices.for_each([&] (Device &dev) {
		if (!cap.valid() && matches(dev) && !dev.owned())
			cap = _acquire(dev); });

	return cap;
}


void Session_component::release_device(Capability<Platform::Device_interface> device_cap)
{
	if (!device_cap.valid())
		return;

	_device_registry.for_each([&] (Device_component &dc) {
		if (device_cap.local_name() == dc.cap().local_name())
			_release_device(dc); });
}


Genode::Ram_dataspace_capability
Session_component::alloc_dma_buffer(size_t const size, Cache cache)
{
	struct Guard {

		Accounted_ram_allocator &_env_ram;
		Heap                    &_heap;
		Io_mmu::Domain          &_domain;
		bool                     _cleanup { true };

		Ram_dataspace_capability ram_cap { };

		struct {
			Dma_buffer * buf { nullptr };
		};

		void disarm() { _cleanup = false; }

		Guard(Accounted_ram_allocator &env_ram,
		      Heap                    &heap,
		      Io_mmu::Domain          &domain)
		:
			_env_ram(env_ram), _heap(heap), _domain(domain)
		{ }

		~Guard()
		{
			if (_cleanup && buf) {
				/* make sure to remove buffer range from domain */
				_domain.remove_range({ buf->dma_addr, buf->size });
				destroy(_heap, buf);
			}

			if (_cleanup && ram_cap.valid())
				_env_ram.free(ram_cap);
		}
	} guard { _env_ram, heap(), _domain };

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
		Dma_buffer &buf = _dma_allocator.alloc_buffer(guard.ram_cap,
		                                              _env.pd().dma_addr(guard.ram_cap),
		                                              _env.pd().ram_size(guard.ram_cap),
		                                              _dma_remapable());
		guard.buf = &buf;

		_domain.add_range({ buf.dma_addr, buf.size }, buf.phys_addr, buf.cap);
	} catch (Dma_allocator::Out_of_virtual_memory) { }

	guard.disarm();
	return guard.ram_cap;
}


void Session_component::free_dma_buffer(Ram_dataspace_capability ram_cap)
{
	if (!ram_cap.valid()) { return; }

	_dma_allocator.buffer_registry().for_each([&] (Dma_buffer &buf) {
		if (buf.cap.local_name() == ram_cap.local_name())
			_free_dma_buffer(buf); });
}


Genode::addr_t Session_component::dma_addr(Ram_dataspace_capability ram_cap)
{
	addr_t ret = 0;

	if (!ram_cap.valid())
		return ret;

	_dma_allocator.buffer_registry().for_each([&] (Dma_buffer const &buf) {
		if (buf.cap.local_name() == ram_cap.local_name())
			ret = buf.dma_addr; });

	return ret;
}


Session_component::Session_component(Env                          &env,
                                     Attached_rom_dataspace const &config,
                                     Device_model                 &devices,
                                     Session_registry             &registry,
                                     Label          const         &label,
                                     Resources      const         &resources,
                                     bool           const          info,
                                     Policy_version const          version)
:
	Session_object<Platform::Session>(env.ep(), resources, label),
	Session_registry::Element(registry, *this),
	Dynamic_rom_session::Producer("devices"),
	_env(env), _config(config), _devices(devices),
	_info(info), _version(version),
	_dma_allocator(_md_alloc),
	_domain(_create_domain())
{
	/*
	 * FIXME: As the ROM session does not propagate Out_of_*
	 *        exceptions resp. does not account costs for the ROM
	 *        dataspace to the client for the moment, we cannot do
	 *        so in the dynamic rom session, and cannot use the
	 *        accounted ram allocator within it. Therefore,
	 *        we account the costs here until the ROM session interface
	 *        changes.
	 */
	if (!_cap_quota_guard().try_withdraw(Cap_quota{Rom_session::CAP_QUOTA}))
		throw Out_of_caps();
	if (!_ram_quota_guard().try_withdraw(Ram_quota{5*1024}))
		throw Out_of_ram();

	/*
	 * Iterate matching devices and reserve reserved memory regions at DMA
	 * allocator.
	 */
	_devices.for_each([&] (Device const &dev) {
		if (!matches(dev)) return;

		dev.for_each_reserved_memory([&] (unsigned, Io_mmu::Range range) {
			_dma_allocator.reserve(range.start, range.size); });
	});
}


Session_component::~Session_component()
{
	_device_registry.for_each([&] (Device_component &dc) {
		_release_device(dc); });

	/* free up dma buffers */
	_dma_allocator.buffer_registry().for_each([&] (Dma_buffer &buf) {
		_free_dma_buffer(buf); });

	/* replenish quota for rom sessions, see constructor for explanation */
	_cap_quota_guard().replenish(Cap_quota{Rom_session::CAP_QUOTA});
	_ram_quota_guard().replenish(Ram_quota{5*1024});
}

/*
 * \brief  Platform driver for ARM device component
 * \author Stefan Kalkowski
 * \date   2020-04-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <device.h>
#include <device_component.h>
#include <pci.h>
#include <session_component.h>
#include <shared_irq.h>

using Driver::Device_component;


void Driver::Device_component::_release_resources()
{
	_io_mem_registry.for_each([&] (Io_mem & iomem) {
		destroy(_session.heap(), &iomem); });

	_irq_registry.for_each([&] (Irq & irq) {
		destroy(_session.heap(), &irq); });

	_io_port_range_registry.for_each([&] (Io_port_range & iop) {
		destroy(_session.heap(), &iop); });

	/* remove reserved memory ranges from IOMMU domains */
	_session.domain_registry().for_each_domain(
		[&] (Driver::Io_mmu::Domain & domain) {
			_reserved_mem_registry.for_each([&] (Io_mem & iomem) {
				domain.remove_range(iomem.range);
		});
	});

	_reserved_mem_registry.for_each([&] (Io_mem & iomem) {
		/* unreserve at dma allocator */
		_session.dma_allocator().unreserve(iomem.range.start, iomem.range.size);

		destroy(_session.heap(), &iomem);
	});

	if (_pci_config.constructed()) _pci_config.destruct();

	_session.ram_quota_guard().replenish(Ram_quota{_ram_quota});
	_session.cap_quota_guard().replenish(Cap_quota{_cap_quota});
}


Driver::Device::Name Device_component::device() const { return _device; }


Driver::Session_component & Device_component::session() { return _session; }


unsigned Device_component::io_mem_index(Device::Pci_bar bar)
{
	unsigned ret = ~0U;
	_io_mem_registry.for_each([&] (Io_mem & iomem) {
		if (iomem.bar.number == bar.number) ret = iomem.idx; });
	return ret;
}


Genode::Io_mem_session_capability
Device_component::io_mem(unsigned idx, Range &range)
{
	Io_mem_session_capability cap;

	_io_mem_registry.for_each([&] (Io_mem & iomem)
	{
		if (iomem.idx != idx)
			return;

		try {
			if (!iomem.io_mem.constructed())
				iomem.io_mem.construct(_env,
				                       iomem.range.start,
				                       iomem.range.size,
				                       iomem.prefetchable);

			range = iomem.range;
			range.start &= 0xfff;
			cap = iomem.io_mem->cap();
		} catch (Genode::Service_denied) { }
	});

	return cap;
}


Genode::Irq_session_capability Device_component::irq(unsigned idx)
{
	Irq_session_capability cap;

	_irq_registry.for_each([&] (Irq & irq)
	{
		if (irq.idx != idx)
			return;

		if (!irq.shared && !irq.irq.constructed()) {
			addr_t pci_cfg_addr = 0;
			if (irq.type != Irq_session::TYPE_LEGACY) {
				if (_pci_config.constructed()) pci_cfg_addr = _pci_config->addr;
				else
					error("MSI(-x) detected for device without pci-config!");

				irq.irq.construct(_env, irq.number, pci_cfg_addr, irq.type);
			} else
				irq.irq.construct(_env, irq.number, irq.mode, irq.polarity);

			Irq_session::Info info = irq.irq->info();
			if (pci_cfg_addr && info.type == Irq_session::Info::MSI)
				pci_msi_enable(_env, *this, pci_cfg_addr, info, irq.type);
		}

		if (irq.shared && !irq.sirq.constructed())
			_device_model.with_shared_irq(irq.number,
			                              [&] (Shared_interrupt & sirq) {
				irq.sirq.construct(_env.ep().rpc_ep(), sirq,
				                   irq.mode, irq.polarity);
			});

		cap = irq.shared ? irq.sirq->cap() : irq.irq->cap();
	});

	return cap;
}


Genode::Io_port_session_capability Device_component::io_port_range(unsigned idx)
{
	Io_port_session_capability cap;

	_io_port_range_registry.for_each([&] (Io_port_range & ipr)
	{
		if (ipr.idx != idx)
			return;

		if (!ipr.io_port_range.constructed())
			ipr.io_port_range.construct(_env, ipr.range.addr,
			                            ipr.range.size);

		cap = ipr.io_port_range->cap();
	});

	return cap;
}


Device_component::Device_component(Registry<Device_component> & registry,
                                   Env                        & env,
                                   Driver::Session_component  & session,
                                   Driver::Device_model       & model,
                                   Driver::Device             & device)
:
	_env(env),
	_session(session),
	_device_model(model),
	_device(device.name()),
	_reg_elem(registry, *this)
{
	session.cap_quota_guard().withdraw(Cap_quota{1});
	_cap_quota += 1;

	/**
	 * FIXME: By now, we cannot use the connection objects for IRQ and
	 *        IOMEM and propagate missing resources when opening the
	 *        sessions, because the combination of Genode::Env and
	 *        Genode::Connection object transparently does quota upgrades.
	 *        If we like to account those costs per client, we actually
	 *        need to explicitely forward exceptions during session
	 *        requests.
	 *        For now, we try to measure the probable costs.
	 */

	try {
		device.for_each_irq([&] (unsigned              idx,
		                         unsigned              nr,
		                         Irq_session::Type     type,
		                         Irq_session::Polarity polarity,
		                         Irq_session::Trigger  mode,
		                         bool                  shared)
		{
			session.ram_quota_guard().withdraw(Ram_quota{Irq_session::RAM_QUOTA});
			_ram_quota += Irq_session::RAM_QUOTA;
			session.cap_quota_guard().withdraw(Cap_quota{Irq_session::CAP_QUOTA});
			_cap_quota += Irq_session::CAP_QUOTA;
			new (session.heap()) Irq(_irq_registry, idx, nr, type, polarity,
			                         mode, shared);
		});

		device.for_each_io_mem([&] (unsigned idx, Range range,
		                            Device::Pci_bar bar, bool pf)
		{
			session.ram_quota_guard().withdraw(Ram_quota{Io_mem_session::RAM_QUOTA});
			_ram_quota += Io_mem_session::RAM_QUOTA;
			session.cap_quota_guard().withdraw(Cap_quota{Io_mem_session::CAP_QUOTA});
			_cap_quota += Io_mem_session::CAP_QUOTA;
			new (session.heap()) Io_mem(_io_mem_registry, bar, idx, range, pf);
		});

		device.for_each_io_port_range([&] (unsigned idx, Io_port_range::Range range,
		                                   Device::Pci_bar)
		{
			session.ram_quota_guard().withdraw(Ram_quota{Io_port_session::RAM_QUOTA});
			_ram_quota += Io_port_session::RAM_QUOTA;
			session.cap_quota_guard().withdraw(Cap_quota{Io_port_session::CAP_QUOTA});
			_cap_quota += Io_port_session::CAP_QUOTA;
			new (session.heap()) Io_port_range(_io_port_range_registry, idx, range);
		});

		device.for_pci_config([&] (Device::Pci_config const & cfg)
		{
			session.ram_quota_guard().withdraw(Ram_quota{Io_mem_session::RAM_QUOTA});
			_ram_quota += Io_mem_session::RAM_QUOTA;
			session.cap_quota_guard().withdraw(Cap_quota{Io_mem_session::CAP_QUOTA});
			_cap_quota += Io_mem_session::CAP_QUOTA;
			_pci_config.construct(cfg.addr);
		});

		device.for_each_reserved_memory([&] (unsigned idx, Range range)
		{
			session.ram_quota_guard().withdraw(Ram_quota{Io_mem_session::RAM_QUOTA});
			_ram_quota += Io_mem_session::RAM_QUOTA;
			session.cap_quota_guard().withdraw(Cap_quota{Io_mem_session::CAP_QUOTA});
			_cap_quota += Io_mem_session::CAP_QUOTA;
			Io_mem & iomem = *(new (session.heap())
				Io_mem(_reserved_mem_registry, {0}, idx, range, false));
			iomem.io_mem.construct(_env, iomem.range.start,
			                       iomem.range.size, false);

			/*  reserve memory at dma allocator */
			session.dma_allocator().reserve(iomem.range.start, iomem.range.size);
		});

		auto add_range_fn = [&] (Driver::Io_mmu::Domain & domain) {
			_reserved_mem_registry.for_each([&] (Io_mem & iomem) {
				domain.add_range(iomem.range, iomem.range.start,
				                 iomem.io_mem->dataspace());
			});
		};

		auto default_domain_fn = [&] () {
			session.domain_registry().with_default_domain(add_range_fn); };

		/* attach reserved memory ranges to IOMMU domains */
		device.for_each_io_mmu(
			/* non-empty list fn */
			[&] (Driver::Device::Io_mmu const &io_mmu) {
				session.domain_registry().with_domain(io_mmu.name,
				                                      add_range_fn,
				                                      default_domain_fn); },

			/* empty list fn */
			default_domain_fn
		);

	} catch(...) {
		_release_resources();
		throw;
	}
}


Device_component::~Device_component() { _release_resources(); };

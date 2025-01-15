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

		/* unmap IRQ from corresponding remapping table */
		if (irq.type == Irq_session::TYPE_LEGACY) {
			_session.irq_controller_registry().for_each([&] (Irq_controller & controller) {
				if (!controller.handles_irq(irq.number)) return;

				_session.with_io_mmu(controller.iommu(), [&] (Driver::Io_mmu & io_mmu_dev) {
					io_mmu_dev.unmap_irq(controller.bdf(), irq.remapped_nbr);
				});
			});
		} else {
			_io_mmu_registry.for_each([&] (Io_mmu & io_mmu) {
				if (_pci_config.constructed())
					_session.with_io_mmu(io_mmu.name, [&] (Driver::Io_mmu & io_mmu_dev) {
						io_mmu_dev.unmap_irq(_pci_config->bdf, irq.remapped_nbr); });
			});
		}

		destroy(_session.heap(), &irq);
	});

	_io_port_range_registry.for_each([&] (Io_port_range & iop) {
		destroy(_session.heap(), &iop); });

	_io_mmu_registry.for_each([&] (Io_mmu & io_mmu) {
		_session.domain_registry().with_domain(io_mmu.name,
			[&] (Driver::Io_mmu::Domain & domain) {

				/* remove reserved memory ranges from IOMMU domains */
				_reserved_mem_registry.for_each([&] (Io_mem & iomem) {
					domain.remove_range(iomem.range); });

			},
			[&] () {} /* no match */
		);

		destroy(_session.heap(), &io_mmu);
	});

	_reserved_mem_registry.for_each([&] (Io_mem & iomem) {
		destroy(_session.heap(), &iomem); });

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
	using Irq_config = Irq_controller::Irq_config;

	Irq_session_capability cap;

	auto remapped_irq = [&] (Device::Name      const & iommu_name,
	                         Pci::Bdf          const & bdf,
	                         Irq                     & irq,
	                         Irq_session::Info const & info,
	                         Irq_config        const & config)
	{
		using Irq_info = Driver::Io_mmu::Irq_info;
		Irq_info remapped_irq { Irq_info::DIRECT, info, irq.number };

		auto map_fn = [&] (Device::Name const & name) {
			_session.with_io_mmu(name, [&] (Driver::Io_mmu & io_mmu) {
				remapped_irq = io_mmu.map_irq(bdf, remapped_irq, config);
			});
		};

		/* for legacy IRQs, take IOMMU referenced by IOAPIC */
		if (iommu_name != "")
			map_fn(iommu_name);
		else
			_io_mmu_registry.for_each([&] (Io_mmu const & io_mmu) {
				map_fn(io_mmu.name);
			});

		/* store remapped number at irq object */
		irq.remapped_nbr = remapped_irq.irq_number;

		return remapped_irq;
	};

	try {
		_irq_registry.for_each([&] (Irq & irq)
		{
			if (irq.idx != idx)
				return;

			if (!irq.shared && !irq.irq.constructed()) {
				addr_t   pci_cfg_addr = 0;
				Pci::Bdf bdf { 0, 0, 0 };
				if (irq.type != Irq_session::TYPE_LEGACY) {
					if (_pci_config.constructed()) {
						pci_cfg_addr = _pci_config->addr;
						bdf          = _pci_config->bdf;
					} else
						error("MSI(-x) detected for device without pci-config!");

					irq.irq.construct(_env, irq.number, pci_cfg_addr, irq.type);
				} else
					irq.irq.construct(_env, irq.number, irq.mode, irq.polarity);

				/**
				 * Core/Kernel is and remains in control of the IRQ controller. When
				 * IRQ remapping is enabled, however, we need to modify the upper 32bit
				 * of the corresponding redirection table entry. This is save for
				 * base-hw as it never touches the upper 32bit after the initial setup.
				 */
				Irq_session::Info info = irq.irq->info();
				if (pci_cfg_addr && info.type == Irq_session::Info::MSI)
					pci_msi_enable(_env, *this, pci_cfg_addr,
					               remapped_irq("", bdf, irq, info, Irq_config::Invalid()).session_info,
					               irq.type);
				else
					_session.irq_controller_registry().for_each([&] (Irq_controller & controller) {
						if (!controller.handles_irq(irq.number)) return;

						remapped_irq(controller.iommu(), controller.bdf(), irq, info,
						             controller.irq_config(irq.number));
						controller.remap_irq(irq.number, irq.remapped_nbr);
					});
			}

			if (irq.shared && !irq.sirq.constructed())
				_device_model.with_shared_irq(irq.number,
				                              [&] (Shared_interrupt & sirq) {
					irq.sirq.construct(_env.ep().rpc_ep(), sirq,
					                   irq.mode, irq.polarity);

					_session.irq_controller_registry().for_each([&] (Irq_controller & controller) {
						if (!controller.handles_irq(irq.number)) return;

						remapped_irq(controller.iommu(), controller.bdf(), irq,
						             { Irq_session::Info::INVALID, 0, 0 },
						             controller.irq_config(irq.number));
						controller.remap_irq(irq.number, irq.remapped_nbr);
					});
				});

			cap = irq.shared ? irq.sirq->cap() : irq.irq->cap();
		});
	} catch (Service_denied) { error("irq could not be setup ", _device); }

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
			Pci::Bdf bdf { cfg.bus_num, cfg.dev_num, cfg.func_num };
			_pci_config.construct(cfg.addr, bdf);
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
				                                      [&] () { });
				/* save IOMMU names for this device */
				new (session.heap()) Io_mmu(_io_mmu_registry, io_mmu.name);
			},

			/* empty list fn */
			default_domain_fn
		);

	} catch(...) {
		_release_resources();
		throw;
	}
}


Device_component::~Device_component() { _release_resources(); };

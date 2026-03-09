/*
 * \brief  Platform driver - device component
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

Genode::Irq_session_capability Device_component::Irq::map(Device_component &dc)
{
	if (irq.constructed())
	   return irq->cap();

	if (sirq.constructed())
	   return sirq->cap();

	using Irq_info   = Driver::Io_mmu::Irq_info;
	using Irq_config = Irq_controller::Irq_config;

	auto remap = [&] (Device::Name      const &iommu_name,
	                  Pci::Bdf          const &bdf,
	                  Irq_session::Info const &info,
	                  Irq_config        const &config)
	{
		Irq_info irq_info { Irq_info::DIRECT, info, number };

		dc._devices.with_io_mmu(iommu_name, [&] (auto &io_mmu) {
			irq_info = io_mmu.map_irq(bdf, irq_info, config); });

		/* store remapped number at irq object */
		remapped_nbr = irq_info.irq_number;

		return irq_info;
	};

	auto remap_legacy_irq = [&] ()
	{
		dc._devices.for_each_irq_controller([&] (auto &controller) {
			if (!controller.handles_irq(number)) return;

			Irq_session::Info const dummy_info {
				Irq_session::Info::INVALID, 0, 0 };
			remap(controller.iommu(), controller.bdf(),
			      dummy_info, controller.irq_config(number));

			/*
			 * Core/Kernel is and remains in control of the IRQ controller. When
			 * IRQ remapping is enabled, however, we need to modify the upper 32bit
			 * of the corresponding redirection table entry. This is save for
			 * base-hw as it never touches the upper 32bit after the initial setup.
			 */
			controller.remap_irq(number, remapped_nbr);
		});
	};

	/* Shared legacy interrupt */
	if (shared) {
		dc._devices.with_shared_irq(number, [&] (auto &shared_irq) {
			sirq.construct(dc._env.ep().rpc_ep(), shared_irq, mode, polarity);
			remap_legacy_irq();
		});
		return sirq.constructed() ? sirq->cap() : Irq_session_capability();
	}

	/* Non-shared legacy interrupt */
	if (type == Irq_session::TYPE_LEGACY) {
		irq.construct(dc._env, number, mode, polarity);
		remap_legacy_irq();
		return irq->cap();
	}

	/* Non-shared Msi-(x) interrupt */
	dc._with_pci_config([&] (auto &pci_config) {
		irq.construct(dc._env, number, pci_config.addr, type,
		              Pci::Bdf::rid(pci_config.bdf));
		Irq_session::Info info = irq->info();
		dc._with_io_mmu([&] (Io_mmu const &io_mmu) {
			info = remap(io_mmu.name, pci_config.bdf, info,
			             Irq_config::Invalid()).session_info;
		});
		pci_msi_enable(dc._env, dc, pci_config.addr, info, type);
	});

	return irq.constructed() ? irq->cap() : Irq_session_capability();
}


void Device_component::Irq::unmap(Device_component &dc)
{
	if (type == Irq_session::TYPE_LEGACY)
		dc._devices.for_each_irq_controller([&] (auto &controller) {
			if (!controller.handles_irq(number)) return;

			dc._devices.with_io_mmu(controller.iommu(),
			                        [&] (auto &io_mmu) {
				io_mmu.unmap_irq(controller.bdf(), remapped_nbr);
			});
		});
	else
		dc._with_io_mmu([&] (auto &io_mmu) {
			dc._with_pci_config([&] (auto &pci_config) {
				dc._devices.with_io_mmu(io_mmu.name, [&] (auto &io_mmu_dev) {
					io_mmu_dev.unmap_irq(pci_config.bdf, remapped_nbr); });
			});
		});
}


void Driver::Device_component::_release_resources()
{
	_io_mem_registry.for_each([&] (Io_mem &iomem) {
		destroy(_session.heap(), &iomem); });

	_irq_registry.for_each([&] (Irq &irq) {

		/* unmap IRQ from corresponding remapping table */
		irq.unmap(*this);
		destroy(_session.heap(), &irq);
	});

	_io_port_range_registry.for_each([&] (Io_port_range &iop) {
		destroy(_session.heap(), &iop); });

	_io_mmu.destruct();

	_reserved_mem_registry.for_each([&] (Io_mem &iomem)
	{
		_session.with_io_mmu_domain([&] (auto &domain) {
			domain.remove_range(iomem.range);
			_session.for_each_io_mmu([&] (auto &io_mmu) {
				io_mmu.iotlb_flush(domain); });
		});
		destroy(_session.heap(), &iomem);
	});

	_pci_config.destruct();

	_session.ram_quota_guard().replenish(Ram_quota{_ram_quota});
	_session.cap_quota_guard().replenish(Cap_quota{_cap_quota});
}


Driver::Device::Name Device_component::device() const { return _device_name; }


Driver::Session_component & Device_component::session() { return _session; }


unsigned Device_component::io_mem_index(Device::Pci_bar bar)
{
	unsigned ret = ~0U;
	_io_mem_registry.for_each([&] (Io_mem &iomem) {
		if (iomem.bar.number == bar.number) ret = iomem.idx; });
	return ret;
}


Genode::Io_mem_session_capability
Device_component::io_mem(unsigned idx, Range &range)
{
	Io_mem_session_capability cap;

	_io_mem_registry.for_each([&] (Io_mem &iomem)
	{
		if (iomem.idx != idx)
			return;

		try {
			if (!iomem.io_mem.constructed())
				iomem.io_mem.construct(_env,
				                       iomem.range.start,
				                       iomem.range.size,
				                       iomem.write_combined);

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

	try {
		_irq_registry.for_each([&] (Irq &irq)
		{
			if (irq.idx != idx)
				return;

			cap = irq.map(*this);
		});
	} catch (Service_denied) { error("irq could not be setup ", _device_name); }

	return cap;
}


Genode::Io_port_session_capability Device_component::io_port_range(unsigned idx)
{
	Io_port_session_capability cap;

	_io_port_range_registry.for_each([&] (Io_port_range &ipr)
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


template <typename SESSION>
void Device_component::_with_reserved_quota_for_session(Driver::Session_component &session,
                                      auto const &fn)
{
	Cap_quota const caps { SESSION::CAP_QUOTA };
	Ram_quota const ram  { SESSION::RAM_QUOTA };

	session.ram_quota_guard().reserve(ram).with_result(
		[&] (Ram_quota_guard::Reservation &reserved_ram) {
			session.cap_quota_guard().reserve(caps).with_result(
				[&] (Cap_quota_guard::Reservation &reserved_caps) {
					reserved_ram.deallocate  = false;
					reserved_caps.deallocate = false;
					_ram_quota += reserved_ram.amount;
					_cap_quota += reserved_caps.amount;
					fn();
				},
				[&] (Cap_quota_guard::Error) { throw Out_of_caps(); });
		},
		[&] (Ram_quota_guard::Error) { throw Out_of_ram(); });
}


Device_component::Device_component(Registry<Device_component> &registry,
                                   Env                        &env,
                                   Driver::Session_component  &session,
                                   Driver::Device_model       &model,
                                   Driver::Device             &device)
:
	_env(env),
	_session(session),
	_devices(model),
	_device_name(device.name()),
	_reg_elem(registry, *this)
{
	if (!session.cap_quota_guard().try_withdraw(Cap_quota{1}))
		throw Out_of_caps();

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
			_with_reserved_quota_for_session<Irq_session>(session, [&] {
				new (session.heap())
					Irq(_irq_registry, idx, nr, type, polarity, mode, shared); });
		});

		device.for_each_io_mem([&] (unsigned idx, Range range,
		                            Device::Pci_bar bar, bool wc)
		{
			_with_reserved_quota_for_session<Io_mem_session>(session, [&] {
				new (session.heap())
					Io_mem(_io_mem_registry, bar, idx, range, wc); });
		});

		device.for_each_io_port_range([&] (unsigned idx, Io_port_range::Range range,
		                                   Device::Pci_bar)
		{
			_with_reserved_quota_for_session<Io_port_session>(session, [&] {
				new (session.heap())
					Io_port_range(_io_port_range_registry, idx, range); });
		});

		device.with_pci_config([&] (Device::Pci_config const &cfg)
		{
			_with_reserved_quota_for_session<Io_mem_session>(session, [&] {
				Pci::Bdf bdf { cfg.bus_num, cfg.dev_num, cfg.func_num };
				_pci_config.construct(cfg.addr, bdf); });
		});

		device.for_each_reserved_memory([&] (unsigned idx, Range range)
		{
			_with_reserved_quota_for_session<Io_mem_session>(session, [&] {
				Io_mem &iomem = *(new (session.heap())
					Io_mem(_reserved_mem_registry, {0}, idx, range, false));
				iomem.io_mem.construct(_env, iomem.range.start,
				                       iomem.range.size, false);
				session.with_io_mmu_domain([&] (auto &domain) {
					domain.add_range(iomem.range, iomem.range.start,
					                 iomem.io_mem->dataspace()).with_error(
						[] (auto err) {
							if (err == decltype(err)::OUT_OF_RAM)
								throw Out_of_ram();
							if (err == decltype(err)::OUT_OF_CAPS)
								throw Out_of_caps();
					});
				});
			});
		});

		device.with_io_mmu([&] (Driver::Device::Io_mmu const &io_mmu) {
			_io_mmu.construct(io_mmu.name);
		});

	} catch(...) {
		_release_resources();
		throw;
	}
}


Device_component::~Device_component() { _release_resources(); };

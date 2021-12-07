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
#include <session_component.h>

using Driver::Device_component;


void Driver::Device_component::_release_resources()
{
	_io_mem_registry.for_each([&] (Io_mem & iomem) {
		destroy(_session.heap(), &iomem); });

	_irq_registry.for_each([&] (Irq & irq) {
		destroy(_session.heap(), &irq); });

	_session.ram_quota_guard().replenish(Ram_quota{_ram_quota});
	_session.cap_quota_guard().replenish(Cap_quota{_cap_quota});
}


Driver::Device::Name Device_component::device() const { return _device; }


Driver::Session_component & Device_component::session() { return _session; }


Genode::Io_mem_session_capability
Device_component::io_mem(unsigned idx, Range &range, Cache cache)
{
	Io_mem_session_capability cap;

	_io_mem_registry.for_each([&] (Io_mem & iomem)
	{
		if (iomem.idx != idx)
			return;

		if (!iomem.io_mem.constructed())
			iomem.io_mem.construct(_session.env(),
			                       iomem.range.start,
			                       iomem.range.size,
			                       cache == WRITE_COMBINED);

		range = iomem.range;
		range.start &= 0xfff;
		cap = iomem.io_mem->cap();
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

		if (!irq.irq.constructed())
			irq.irq.construct(_session.env(), irq.number);

		cap = irq.irq->cap();
	});

	return cap;
}


Device_component::Device_component(Registry<Device_component> & registry,
                                   Driver::Session_component  & session,
                                   Driver::Device             & device)
: _session(session), _device(device.name()), _reg_elem(registry, *this)
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
		device.for_each_irq([&] (unsigned idx, unsigned nr)
		{
			session.ram_quota_guard().withdraw(Ram_quota{Irq_session::RAM_QUOTA});
			_ram_quota += Irq_session::RAM_QUOTA;
			session.cap_quota_guard().withdraw(Cap_quota{Irq_session::CAP_QUOTA});
			_cap_quota += Irq_session::CAP_QUOTA;
			new (session.heap()) Irq(_irq_registry, idx, nr);
		});
	
		device.for_each_io_mem([&] (unsigned idx, Range range)
		{
			session.ram_quota_guard().withdraw(Ram_quota{Io_mem_session::RAM_QUOTA});
			_ram_quota += Io_mem_session::RAM_QUOTA;
			session.cap_quota_guard().withdraw(Cap_quota{Io_mem_session::CAP_QUOTA});
			_cap_quota += Io_mem_session::CAP_QUOTA;
			new (session.heap()) Io_mem(_io_mem_registry, idx, range);
		});
	} catch(...) {
		_release_resources();
		throw;
	}
}


Device_component::~Device_component() { _release_resources(); };

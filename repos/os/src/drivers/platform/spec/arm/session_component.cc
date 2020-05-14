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
#include <session_component.h>

using Driver::Session_component;

void Session_component::produce_xml(Genode::Xml_generator &xml)
{
	for (Device_list_element * e = _device_list.first(); e; e = e->next()) {
		e->object()->report(xml); }
}


Genode::Heap & Session_component::heap() { return _md_alloc; }


Genode::Env & Session_component::env() { return _env; }


Driver::Device_model & Session_component::devices() { return _device_model; }


void Session_component::add(Device::Name const & device)
{
	if (has_device(device)) return;

	Device_component * new_dc =
		new (_md_alloc) Device_component(*this, device);

	Device_list_element * last = nullptr;
	for (last = _device_list.first(); last; last = last->next()) {
		if (!last->next()) break; }
	_device_list.insert(&new_dc->_list_elem, last);
}


bool Session_component::has_device(Device::Name const & device) const
{
	for (Device_list_element const * e = _device_list.first(); e;
	     e = e->next()) {
		if (e->object()->device() == device) { return true; }
	}
	return false;
}


unsigned Session_component::devices_count() const
{
	unsigned counter = 0;
	for (Device_list_element const * e = _device_list.first();
	     e; e = e->next()) { counter++; }
	return counter;
}


Genode::Rom_session_capability Session_component::devices_rom() {
	return _rom_session.cap(); }


Platform::Device_capability
Session_component::acquire_device(Platform::Session::String const &name)
{
	for (Device_list_element * e = _device_list.first(); e; e = e->next()) {
		if (e->object()->device() != name.string()) { continue; }

		if (!e->object()->acquire()) {
			Genode::error("Device ", e->object()->device(),
			              " already acquired!");
			break;
		}

		/* account one device capability needed */
		_cap_quota_guard().replenish(Genode::Cap_quota{1});

		return _env.ep().rpc_ep().manage(e->object());
	}

	return Platform::Device_capability();
}


void Session_component::release_device(Platform::Device_capability device_cap)
{
	_env.ep().rpc_ep().apply(device_cap, [&] (Device_component * dc) {
		_env.ep().rpc_ep().dissolve(dc);
		_cap_quota_guard().replenish(Genode::Cap_quota{1});
		dc->release();
	});
}


Genode::Ram_dataspace_capability
Session_component::alloc_dma_buffer(Genode::size_t const size)
{
	Genode::Ram_dataspace_capability ram_cap =
		_env_ram.alloc(size, Genode::UNCACHED);

	if (!ram_cap.valid()) return ram_cap;

	try {
		_buffer_list.insert(new (_md_alloc) Dma_buffer(ram_cap));
	} catch (Genode::Out_of_ram)  {
		_env_ram.free(ram_cap);
		throw;
	} catch (Genode::Out_of_caps) {
		_env_ram.free(ram_cap);
		throw;
	}

	return ram_cap;
}


void Session_component::free_dma_buffer(Genode::Ram_dataspace_capability ram_cap)
{
	if (!ram_cap.valid()) { return; }

	for (Dma_buffer * buf = _buffer_list.first(); buf; buf = buf->next()) {

		if (buf->cap.local_name() != ram_cap.local_name()) continue;

		_buffer_list.remove(buf);
		destroy(_md_alloc, buf);
		_env_ram.free(ram_cap);
		return;
	}
}


Genode::addr_t Session_component::bus_addr_dma_buffer(Ram_dataspace_capability ram_cap)
{
	if (!ram_cap.valid()) { return 0; }

	for (Dma_buffer * buf = _buffer_list.first(); buf; buf = buf->next()) {

		if (buf->cap.local_name() != ram_cap.local_name()) continue;

		Genode::Dataspace_client dsc(buf->cap);
		return dsc.phys_addr();
	}

	return 0;
}


Session_component::Session_component(Genode::Env       & env,
                                     Device_model      & devices,
                                     Registry          & registry,
                                     Label       const & label,
                                     Resources   const & resources,
                                     Diag        const & diag)
: Genode::Session_object<Platform::Session>(env.ep(), resources,
                                            label, diag),
  Registry::Element(registry, *this),
  Genode::Dynamic_rom_session::Xml_producer("devices"),
  _env(env),
  _device_model(devices)
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
	_cap_quota_guard().withdraw(Genode::Cap_quota{1});
	_ram_quota_guard().withdraw(Genode::Ram_quota{5*1024});
}


Session_component::~Session_component()
{
	while (_device_list.first()) {
		Device_list_element * e = _device_list.first();
		_device_list.remove(e);
		destroy(_md_alloc, e->object());
	}

	/* replenish quota for rom sessions, see constructor for explanation */
	_cap_quota_guard().replenish(Genode::Cap_quota{1});
	_ram_quota_guard().replenish(Genode::Ram_quota{5*1024});
}

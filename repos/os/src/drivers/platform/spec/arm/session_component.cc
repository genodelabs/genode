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

void Session_component::produce_xml(Xml_generator &xml)
{
	if (!_info) { return; }
	for (Device_list_element * e = _device_list.first(); e; e = e->next()) {
		e->object()->report(xml); }
}


Genode::Heap & Session_component::heap() { return _md_alloc; }


Driver::Env & Session_component::env() { return _env; }


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


void Session_component::update_devices_rom()
{
	_rom_session.trigger_update();
}


Genode::Rom_session_capability Session_component::devices_rom() {
	return _rom_session.cap(); }


Genode::Capability<Platform::Device_interface>
Session_component::acquire_device(Platform::Session::Device_name const &name)
{
	for (Device_list_element * e = _device_list.first(); e; e = e->next()) {
		if (e->object()->device() != name.string()) { continue; }

		if (!e->object()->acquire()) {
			error("Device ", e->object()->device(),
			              " already acquired!");
			break;
		}

		/* account one device capability needed */
		_cap_quota_guard().replenish(Cap_quota{1});

		return _env.env.ep().rpc_ep().manage(e->object());
	}

	return Capability<Platform::Device_interface>();
}


Genode::Capability<Platform::Device_interface>
Session_component::acquire_single_device()
{
	Device_list_element * e = _device_list.first();
	if (!e) { return Capability<Platform::Device_interface>(); }

	return acquire_device(e->object()->device());
}


void Session_component::release_device(Capability<Platform::Device_interface> device_cap)
{
	_env.env.ep().rpc_ep().apply(device_cap, [&] (Device_component * dc) {
		_env.env.ep().rpc_ep().dissolve(dc);
		_cap_quota_guard().replenish(Cap_quota{1});
		dc->release();
	});
}


Genode::Ram_dataspace_capability
Session_component::alloc_dma_buffer(size_t const size, Cache cache)
{
	Ram_dataspace_capability ram_cap = _env_ram.alloc(size, cache);

	if (!ram_cap.valid()) return ram_cap;

	try {
		_buffer_list.insert(new (_md_alloc) Dma_buffer(ram_cap));
	} catch (Out_of_ram)  {
		_env_ram.free(ram_cap);
		throw;
	} catch (Out_of_caps) {
		_env_ram.free(ram_cap);
		throw;
	}

	return ram_cap;
}


void Session_component::free_dma_buffer(Ram_dataspace_capability ram_cap)
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


Genode::addr_t Session_component::dma_addr(Ram_dataspace_capability ram_cap)
{
	if (!ram_cap.valid()) { return 0; }

	for (Dma_buffer * buf = _buffer_list.first(); buf; buf = buf->next()) {

		if (buf->cap.local_name() != ram_cap.local_name()) continue;

		Dataspace_client dsc(buf->cap);
		return dsc.phys_addr();
	}

	return 0;
}


Session_component::Session_component(Driver::Env       & env,
                                     Session_registry  & registry,
                                     Label       const & label,
                                     Resources   const & resources,
                                     Diag        const & diag,
                                     bool        const   info)
: Session_object<Platform::Session>(env.env.ep(), resources, label, diag),
  Session_registry::Element(registry, *this),
  Dynamic_rom_session::Xml_producer("devices"),
  _env(env), _info(info)
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
	_cap_quota_guard().withdraw(Cap_quota{1});
	_ram_quota_guard().withdraw(Ram_quota{5*1024});
}


Session_component::~Session_component()
{
	while (_device_list.first()) {
		Device_list_element * e = _device_list.first();
		_device_list.remove(e);
		destroy(_md_alloc, e->object());
	}

	/* replenish quota for rom sessions, see constructor for explanation */
	_cap_quota_guard().replenish(Cap_quota{1});
	_ram_quota_guard().replenish(Ram_quota{5*1024});
}

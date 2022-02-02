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


Genode::Capability<Platform::Device_interface>
Session_component::_acquire(Device & device)
{
	Device_component * dc = new (heap())
		Device_component(_device_registry, *this, device);
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
}


void Session_component::_free_dma_buffer(Dma_buffer & buf)
{
	Ram_dataspace_capability cap = buf.cap;
	destroy(heap(), &buf);
	_env_ram.free(cap);
}


bool Session_component::matches(Device & dev) const
{
	bool ret = false;

	try {
		Session_policy const policy { label(), _config.xml() };
		policy.for_each_sub_node("device", [&] (Xml_node node) {
			if (dev.name() == node.attribute_value("name", Device::Name()))
				ret = true;
		});
	} catch (Session_policy::No_policy_defined) { }

	return ret;
};


void Session_component::update_policy(bool info, Policy_version version)
{
	_info    = info;
	_version = version;

	enum Device_state { AWAY, CHANGED, UNCHANGED };

	_device_registry.for_each([&] (Device_component & dc) {
		Device_state state = AWAY;
		_devices.for_each([&] (Device & dev) {
			if (dev.name() != dc.device())
				return;
			state = (dev.owner() == _owner_id) ? UNCHANGED : CHANGED;
		});

		if (state == UNCHANGED)
			return;

		if (state == AWAY)
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
	if (!_info)
		return;

	if (_version.valid())
		xml.attribute("version", _version);

	_devices.for_each([&] (Device & dev) {
		if (matches(dev)) dev.report(xml, *this); });
}


Genode::Env & Session_component::env() { return _env; }


Driver::Device_model & Session_component::devices() { return _devices; }


Genode::Heap & Session_component::heap() { return _md_alloc; }


void Session_component::update_devices_rom()
{
	_rom_session.trigger_update();
}


Genode::Rom_session_capability Session_component::devices_rom() {
	return _rom_session.cap(); }


Genode::Capability<Platform::Device_interface>
Session_component::acquire_device(Platform::Session::Device_name const &name)
{
	Capability<Platform::Device_interface> cap;

	/* Search for existing, aquired device session */
	_device_registry.for_each([&] (Device_component & dc) {
		if (dc.device() == name)
			cap = dc.cap();
	});

	if (cap.valid())
		return cap;

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

	/* Search for existing, aquired device session */
	_device_registry.for_each([&] (Device_component & dc) {
		cap = dc.cap(); });

	if (cap.valid())
		return cap;

	_devices.for_each([&] (Device & dev) {
		if (matches(dev) && !dev.owner().valid())
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
	Ram_dataspace_capability ram_cap = _env_ram.alloc(size, cache);

	if (!ram_cap.valid()) return ram_cap;

	try {
		new (heap()) Dma_buffer(_buffer_registry, ram_cap);
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

	_buffer_registry.for_each([&] (Dma_buffer & buf) {
		if (buf.cap.local_name() == ram_cap.local_name())
			_free_dma_buffer(buf); });
}


Genode::addr_t Session_component::dma_addr(Ram_dataspace_capability ram_cap)
{
	addr_t ret = 0;

	if (!ram_cap.valid())
		return ret;

	_buffer_registry.for_each([&] (Dma_buffer & buf) {
		if (buf.cap.local_name() == ram_cap.local_name())
			ret = _env.pd().dma_addr(buf.cap); });

	return ret;
}


Session_component::Session_component(Env                    & env,
                                     Attached_rom_dataspace & config,
                                     Device_model           & devices,
                                     Session_registry       & registry,
                                     Label          const   & label,
                                     Resources      const   & resources,
                                     Diag           const   & diag,
                                     bool           const     info,
                                     Policy_version const     version)
:
	Session_object<Platform::Session>(env.ep(), resources, label, diag),
	Session_registry::Element(registry, *this),
	Dynamic_rom_session::Xml_producer("devices"),
	_env(env), _config(config), _devices(devices), _info(info), _version(version)
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
}


Session_component::~Session_component()
{
	_device_registry.for_each([&] (Device_component & dc) {
		_release_device(dc); });

	/* free up dma buffers */
	_buffer_registry.for_each([&] (Dma_buffer & buf) {
		_free_dma_buffer(buf); });

	/* replenish quota for rom sessions, see constructor for explanation */
	_cap_quota_guard().replenish(Cap_quota{Rom_session::CAP_QUOTA});
	_ram_quota_guard().replenish(Ram_quota{5*1024});
}

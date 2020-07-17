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

Driver::Device::Name Device_component::device() const { return _device; }


Driver::Session_component & Device_component::session() { return _session; }


bool Driver::Device_component::acquire()
{
	bool acquired = false;
	_session.env().devices.for_each([&] (Driver::Device & device) {
		if (device.name() == _device) {
			acquired = device.acquire(_session); }});
	return acquired;
}


void Driver::Device_component::release()
{
	_session.env().devices.for_each([&] (Driver::Device & device) {
		if (device.name() == _device) { device.release(_session); }});
}


Genode::Io_mem_session_capability
Device_component::io_mem(unsigned idx, Cache_attribute attr)
{
	Io_mem_session_capability cap;
	_session.env().devices.for_each([&] (Driver::Device & device) {
		if (device.name() == _device) {
			cap = device.io_mem(idx, attr, _session); }});
	return cap;
}


Genode::Irq_session_capability Device_component::irq(unsigned idx)
{
	Irq_session_capability cap;
	_session.env().devices.for_each([&] (Driver::Device & device) {
		if (device.name() == _device) { cap = device.irq(idx, _session); }});
	return cap;
}


void Driver::Device_component::report(Xml_generator & xml)
{
	_session.env().devices.for_each([&] (Driver::Device & device) {
		if (device.name() == _device) { device.report(xml, _session); }});
}


Device_component::Device_component(Driver::Session_component & session,
                                   Driver::Device::Name const  device)
: _session(session), _device(device) { }


Device_component::~Device_component() { release(); }

/*
 * \brief  Platform driver - Device abstraction
 * \author Stefan Kalkowski
 * \date   2020-04-30
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


Driver::Device::Owner::Owner(Session_component & session)
: obj_id(reinterpret_cast<void*>(&session)) {}


Driver::Device::Name Driver::Device::name() const { return _name; }


Driver::Device::Type Driver::Device::type() const { return _type; }


Driver::Device::Owner Driver::Device::owner() const { return _owner; }


void Driver::Device::acquire(Session_component & sc) {
	if (!_owner.valid()) _owner = sc; }


void Driver::Device::release(Session_component & sc) {
	if (_owner == sc) _owner = Owner();; }


void Driver::Device::report(Xml_generator & xml, Session_component & sc)
{
	xml.node("device", [&] () {
		xml.attribute("name", name());
		xml.attribute("type", type());
		_io_mem_list.for_each([&] (Io_mem & io_mem) {
			xml.node("io_mem", [&] () {
				xml.attribute("phys_addr", String<16>(Hex(io_mem.range.start)));
				xml.attribute("size",      String<16>(Hex(io_mem.range.size)));
			});
		});
		_irq_list.for_each([&] (Irq & irq) {
			xml.node("irq", [&] () {
				xml.attribute("number", irq.number);
			});
		});
		_property_list.for_each([&] (Property & p) {
			xml.node("property", [&] () {
				xml.attribute("name",  p.name);
				xml.attribute("value", p.value);
			});
		});
		_report_platform_specifics(xml, sc);
	});
}


Driver::Device::Device(Name name, Type type)
: _name(name), _type(type) { }


Driver::Device::~Device()
{
	if (_owner.valid()) {
		error("Device to be destroyed, still obtained by session"); }
}

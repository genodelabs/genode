/*
 * \brief  Platform driver - Device abstraction for i.MX
 * \author Stefan Kalkowski
 * \date   2020-08-17
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__IMX8MQ__IMX_DEVICE_H_
#define _SRC__DRIVERS__PLATFORM__IMX8MQ__IMX_DEVICE_H_

#include <device.h>

namespace Driver {
	using namespace Genode;

	class  Imx_device;
	struct Clock_update_policy;
	struct Power_domain_update_policy;
	struct Reset_domain_update_policy;
}


class Driver::Imx_device : public Driver::Device
{
	public:

		struct Clock : List_model<Clock>::Element
		{
			using Name  = Genode::String<64>;

			Name          name;
			Name          parent;
			Name          driver_name;
			unsigned long rate;

			Clock(Name name,
			      Name parent,
			      Name driver_name,
			      unsigned long rate)
			: name(name), parent(parent),
			  driver_name(driver_name), rate(rate) {}
		};

		struct Power_domain : List_model<Power_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Power_domain(Name name) : name(name) {}
		};

		struct Reset_domain : List_model<Reset_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Reset_domain(Name name) : name(name) {}
		};

		bool acquire(Session_component &) override;
		void release(Session_component &) override;

		Imx_device(Device::Name name, Device::Type type)
		: Device(name, type) {}

	protected:

		friend class Driver::Device_model;
		friend class List_model<Device>;

		void _report_platform_specifics(Xml_generator &,
		                                Session_component &) override;

		List_model<Clock>        _clock_list {};
		List_model<Power_domain> _power_domain_list {};
		List_model<Reset_domain> _reset_domain_list {};
};


struct Driver::Clock_update_policy
: Genode::List_model<Imx_device::Clock>::Update_policy
{
	Genode::Allocator & alloc;

	Clock_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & clock) {
		Genode::destroy(alloc, &clock); }

	Element & create_element(Genode::Xml_node node)
	{
		Element::Name name   = node.attribute_value("name",        Element::Name());
		Element::Name parent = node.attribute_value("parent",      Element::Name());
		Element::Name driver = node.attribute_value("driver_name", Element::Name());
		unsigned long rate   = node.attribute_value<unsigned long >("rate", 0);
		return *(new (alloc) Element(name, parent, driver, rate));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & clock, Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return name == clock.name;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("clock");
	}
};


struct Driver::Power_domain_update_policy
: Genode::List_model<Imx_device::Power_domain>::Update_policy
{
	Genode::Allocator & alloc;

	Power_domain_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & pd) {
		Genode::destroy(alloc, &pd); }

	Element & create_element(Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return *(new (alloc) Element(name));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & pd, Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return name == pd.name;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("power-domain");
	}
};


struct Driver::Reset_domain_update_policy
: Genode::List_model<Imx_device::Reset_domain>::Update_policy
{
	Genode::Allocator & alloc;

	Reset_domain_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & pd) {
		Genode::destroy(alloc, &pd); }

	Element & create_element(Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return *(new (alloc) Element(name));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & pd, Genode::Xml_node node)
	{
		Element::Name name = node.attribute_value("name", Element::Name());
		return name == pd.name;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("reset-domain");
	}
};

#endif /* _SRC__DRIVERS__PLATFORM__IMX8MQ__IMX_DEVICE_H_ */

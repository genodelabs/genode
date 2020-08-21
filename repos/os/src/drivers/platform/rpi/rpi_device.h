/*
 * \brief  Platform driver - Device abstraction for rpi
 * \author Stefan Kalkowski
 * \date   2020-08-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__IMX8MQ__RPI_DEVICE_H_
#define _SRC__DRIVERS__PLATFORM__IMX8MQ__RPI_DEVICE_H_

#include <device.h>

namespace Driver {
	using namespace Genode;

	class  Rpi_device;
	struct Power_domain_update_policy;
}


class Driver::Rpi_device : public Driver::Device
{
	public:

		struct Power_domain : List_model<Power_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Power_domain(Name name) : name(name) {}

			unsigned id();
		};

		bool acquire(Session_component &) override;
		void release(Session_component &) override;

		Rpi_device(Device::Name name) : Device(name) {}

	protected:

		friend class Driver::Device_model;
		friend class List_model<Device>;

		void _report_platform_specifics(Xml_generator &,
		                                Session_component &) override;

		List_model<Power_domain> _power_domain_list {};
};


struct Driver::Power_domain_update_policy
: Genode::List_model<Rpi_device::Power_domain>::Update_policy
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

#endif /* _SRC__DRIVERS__PLATFORM__IMX8MQ__RPI_DEVICE_H_ */

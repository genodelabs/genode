/*
 * \brief  Platform driver - Device model policy for rpi
 * \author Stefan Kalkowski
 * \date   2020-08-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <env.h>
#include <rpi_device.h>

using Driver::Device_model;
using Driver::Device;
using Driver::Rpi_device;

void Device_model::destroy_element(Device & dev)
{
	Rpi_device & device = static_cast<Rpi_device&>(dev);

	{
		Irq_update_policy policy(_env.heap);
		device._irq_list.destroy_all_elements(policy);
	}

	{
		Io_mem_update_policy policy(_env.heap);
		device._io_mem_list.destroy_all_elements(policy);
	}

	{
		Property_update_policy policy(_env.heap);
		device._property_list.destroy_all_elements(policy);
	}

	{
		Power_domain_update_policy policy(_env.heap);
		device._power_domain_list.destroy_all_elements(policy);
	}

	Genode::destroy(_env.heap, &device);
}


Device & Device_model::create_element(Genode::Xml_node node)
{
	Device::Name name = node.attribute_value("name", Device::Name());
	return *(new (_env.heap) Rpi_device(name));
}


void Device_model::update_element(Device & dev,
                                  Genode::Xml_node node)
{
	Rpi_device & device = static_cast<Rpi_device&>(dev);

	{
		Irq_update_policy policy(_env.heap);
		device._irq_list.update_from_xml(policy, node);
	}

	{
		Io_mem_update_policy policy(_env.heap);
		device._io_mem_list.update_from_xml(policy, node);
	}

	{
		Property_update_policy policy(_env.heap);
		device._property_list.update_from_xml(policy, node);
	}

	{
		Power_domain_update_policy policy(_env.heap);
		device._power_domain_list.update_from_xml(policy, node);
	}
}

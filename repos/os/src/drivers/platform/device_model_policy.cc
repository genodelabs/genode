/*
 * \brief  Platform driver - Device model policy
 * \author Stefan Kalkowski
 * \date   2020-05-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <device.h>

using Driver::Device_model;
using Driver::Device;

void Device_model::destroy_element(Device & device)
{
	{
		Irq_update_policy policy(_heap);
		device._irq_list.destroy_all_elements(policy);
	}

	{
		Io_mem_update_policy policy(_heap);
		device._io_mem_list.destroy_all_elements(policy);
	}

	{
		Io_port_update_policy policy(_heap);
		device._io_port_range_list.destroy_all_elements(policy);
	}

	{
		Property_update_policy policy(_heap);
		device._property_list.destroy_all_elements(policy);
	}

	{
		Clock_update_policy policy(_heap);
		device._clock_list.destroy_all_elements(policy);
	}

	{
		Power_domain_update_policy policy(_heap);
		device._power_domain_list.destroy_all_elements(policy);
	}

	{
		Reset_domain_update_policy policy(_heap);
		device._reset_domain_list.destroy_all_elements(policy);
	}

	{
		Pci_config_update_policy policy(_heap);
		device._pci_config_list.destroy_all_elements(policy);
	}

	{
		Reserved_memory_update_policy policy(_heap);
		device._reserved_mem_list.destroy_all_elements(policy);
	}

	Genode::destroy(_heap, &device);
}


Device & Device_model::create_element(Genode::Xml_node node)
{
	Device::Name name = node.attribute_value("name", Device::Name());
	Device::Type type = node.attribute_value("type", Device::Type());
	return *(new (_heap) Device(_env, *this, name, type));
}


void Device_model::update_element(Device & device,
                                  Genode::Xml_node node)
{
	{
		Irq_update_policy policy(_heap);
		device._irq_list.update_from_xml(policy, node);
	}

	{
		Io_mem_update_policy policy(_heap);
		device._io_mem_list.update_from_xml(policy, node);
	}

	{
		Io_port_update_policy policy(_heap);
		device._io_port_range_list.update_from_xml(policy, node);
	}

	{
		Property_update_policy policy(_heap);
		device._property_list.update_from_xml(policy, node);
	}

	{
		Clock_update_policy policy(_heap);
		device._clock_list.update_from_xml(policy, node);
	}

	{
		Power_domain_update_policy policy(_heap);
		device._power_domain_list.update_from_xml(policy, node);
	}

	{
		Reset_domain_update_policy policy(_heap);
		device._reset_domain_list.update_from_xml(policy, node);
	}

	{
		Pci_config_update_policy policy(_heap);
		device._pci_config_list.update_from_xml(policy, node);
	}

	{
		Reserved_memory_update_policy policy(_heap);
		device._reserved_mem_list.update_from_xml(policy, node);
	}
}

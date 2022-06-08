/*
 * \brief  Replaces driver/acpi/property.c
 * \author Josef Soentgen
 * \date   2022-05-06
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/acpi.h>


static const void * acpi_fwnode_device_get_match_data(const struct fwnode_handle *fwnode, const struct device *dev)
{
	const struct acpi_device *adev = to_acpi_device_node(fwnode);
	if (WARN_ON(!adev))
		return NULL;

	return acpi_device_get_match_data(dev);
}


static const char *acpi_fwnode_get_name(const struct fwnode_handle *fwnode)
{
	const struct acpi_device *adev = to_acpi_device_node(fwnode);
	if (WARN_ON(!adev))
		return NULL;

	return acpi_device_bid(adev);
}


const struct fwnode_operations acpi_device_fwnode_ops = {
	.device_get_match_data = acpi_fwnode_device_get_match_data,
	.get_name              = acpi_fwnode_get_name,
};


bool is_acpi_device_node(const struct fwnode_handle *fwnode)
{
	return !IS_ERR_OR_NULL(fwnode) && fwnode->ops == &acpi_device_fwnode_ops;
}


bool is_acpi_data_node(const struct fwnode_handle *fwnode)
{
	lx_emul_trace(__func__);
	return false;
}


int __acpi_node_get_property_reference(const struct fwnode_handle *fwnode,
                                       const char *propname,
                                       size_t index, size_t num_args,
                                       struct fwnode_reference_args *args)
{
	lx_emul_trace(__func__);
	return -ENOENT;
}



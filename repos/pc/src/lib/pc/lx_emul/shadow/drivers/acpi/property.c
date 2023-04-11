/*
 * \brief  Replaces drivers/acpi/property.c
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


bool is_acpi_device_node(const struct fwnode_handle *fwnode)
{
	lx_emul_trace(__func__);
	return false;
}


bool is_acpi_data_node(const struct fwnode_handle *fwnode)
{
	lx_emul_trace(__func__);
	return false;
}

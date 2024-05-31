/*
 * \brief  Replaces driver/acpi/utils.c
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


bool acpi_check_dsm(acpi_handle handle, const guid_t *guid, u64 rev, u64 funcs)
{
	lx_emul_trace(__func__);
	return false;
}


union acpi_object *acpi_evaluate_dsm(acpi_handle handle, const guid_t *guid,
                                     u64 rev, u64 func, union acpi_object *argv4)
{
	lx_emul_trace(__func__);
	return NULL;
}

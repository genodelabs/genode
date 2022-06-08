/*
 * \brief  Replaces driver/acpi/utils.c
 * \author Josef Soentgen
 * \author Christian Helmuth
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


bool acpi_has_method(acpi_handle handle, char *name)
{
	if (ACPI_COMPARE_NAMESEG(name, METHOD_NAME__CRS))
		return true;

	return false;
}


bool acpi_check_dsm(acpi_handle handle, const guid_t *guid, u64 rev, u64 funcs)
{
	lx_emul_trace(__func__);
	return false;
}


union acpi_object *acpi_evaluate_dsm(acpi_handle handle, const guid_t *guid,
                                     u64 rev, u64 func, union acpi_object *argv4)
{
	acpi_status ret;
	struct acpi_buffer buf = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object params[4];
	struct acpi_object_list input = {
		.count = 4,
		.pointer = params,
	};

	params[0].type = ACPI_TYPE_BUFFER;
	params[0].buffer.length = 16;
	params[0].buffer.pointer = (u8 *)guid;
	params[1].type = ACPI_TYPE_INTEGER;
	params[1].integer.value = rev;
	params[2].type = ACPI_TYPE_INTEGER;
	params[2].integer.value = func;
	if (argv4) {
		params[3] = *argv4;
	} else {
		params[3].type = ACPI_TYPE_PACKAGE;
		params[3].package.count = 0;
		params[3].package.elements = NULL;
	}

	ret = acpi_evaluate_object(handle, "_DSM", &input, &buf);
	if (ACPI_SUCCESS(ret))
		return (union acpi_object *)buf.pointer;

	if (ret != AE_NOT_FOUND)
		acpi_handle_warn(handle,
				"failed to evaluate _DSM (0x%x)\n", ret);

	return NULL;
}


void acpi_handle_printk(const char * level,acpi_handle handle,const char * fmt,...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	printk("ACPI: %pV", &vaf);

	va_end(args);
}

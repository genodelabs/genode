/*
 * \brief  Retrieve compiled in NHLT ACPI table
 *         (This needs to be obtained dynamically from ACPI)
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2022-05-31
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/io_mem.h>

#include <linux/acpi.h>

bool acpi_int_src_ovr[NR_IRQS_LEGACY];

extern int lx_emul_acpi_table(const char * const name, void *ctx,
                              void (*fn) (void *,
                                          unsigned long addr,
                                          unsigned long size));


struct acpi_table_io_mem
{
	unsigned long phys_addr;
	unsigned long size;
};


static void query_acpi_table_io_mem(void *ctx, unsigned long addr, unsigned long size)
{
	struct acpi_table_io_mem *table_ctx =
		(struct acpi_table_io_mem*)ctx;

	table_ctx->phys_addr = addr;
	table_ctx->size      = size;
}


acpi_status
acpi_get_table(char *signature,
               u32 instance, struct acpi_table_header ** out_table)
{
	struct acpi_table_io_mem table = {
		.phys_addr = 0, .size = 0 };

	if (lx_emul_acpi_table("NHLT", &table, query_acpi_table_io_mem)) {
		/* leak NHLT mapping for now */
		void *nhlt = lx_emul_io_mem_map(table.phys_addr,
		                                table.size, 0);
		if (nhlt && strncmp(signature, ACPI_SIG_NHLT, 4) == 0) {
			*out_table = (struct acpi_table_header *)nhlt;
			printk("%s: ACPI table '%s' found at 0%lx size: %lu\n",
			       __func__, signature, table.phys_addr, table.size);
			return AE_OK;
		}
	}

	printk("%s: ACPI table '%s' not found\n",  __func__, signature);
	return AE_NOT_FOUND;
}


acpi_status acpi_resource_to_address64(struct acpi_resource *resource,
                                       struct acpi_resource_address64 *out)
{
	lx_emul_trace_and_stop(__func__);
}


acpi_status acpi_walk_resources(acpi_handle device_handle, char *name,
                                acpi_walk_resource_callback user_function,
                                void *context)
{
	return (AE_OK);
}

/*
 * \brief  Replaces driver/acpi/scan.c
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


enum dev_dma_attr acpi_get_dma_attr(struct acpi_device *adev)
{
    lx_emul_trace(__func__);
	return DEV_DMA_NOT_SUPPORTED;
}


int acpi_dma_configure_id(struct device *dev, enum dev_dma_attr attr, const u32 *input_id)
{
	lx_emul_trace(__func__);
	return -1;
}

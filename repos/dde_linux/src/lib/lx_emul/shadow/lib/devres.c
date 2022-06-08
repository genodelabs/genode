/*
 * \brief  Replaces lib/devres.c
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/device.h>
#include <lx_emul/io_mem.h>

void __iomem *devm_ioremap_resource(struct device *dev,
                                    const struct resource *res)
{
	return lx_emul_io_mem_map(res->start, resource_size(res));
}


void __iomem *devm_ioremap(struct device *dev, resource_size_t offset,
                           resource_size_t size)
{
	return lx_emul_io_mem_map(offset, size);
}


void __iomem * devm_ioremap_uc(struct device *dev, resource_size_t offset,
                               resource_size_t size)
{
	return lx_emul_io_mem_map(offset, size);
}

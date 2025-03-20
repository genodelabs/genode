/*
 * \brief  PCI access
 * \author Stefan Kalkowski
 * \date   2022-07-29
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/pci.h>

resource_size_t __weak pcibios_align_resource(void *data,
                          const struct resource *res,
                          resource_size_t size,
                          resource_size_t align)
{
       return res->start;
}



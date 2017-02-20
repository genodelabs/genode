/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/********************
 ** linux/ioport.h **
 ********************/

#define IORESOURCE_IO  0x00000100
#define IORESOURCE_MEM 0x00000200
#define IORESOURCE_IRQ 0x00000400

struct resource
{
	resource_size_t start;
	resource_size_t end;
	const char     *name;
	unsigned long   flags;
};

struct device;

struct resource *request_region(resource_size_t start, resource_size_t n,
                                const char *name);
struct resource *request_mem_region(resource_size_t start, resource_size_t n,
                                    const char *name);
struct resource * devm_request_mem_region(struct device *dev, resource_size_t start,
                                          resource_size_t n, const char *name);


void release_region(resource_size_t start, resource_size_t n);
void release_mem_region(resource_size_t start, resource_size_t n);

resource_size_t resource_size(const struct resource *res);

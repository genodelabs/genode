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
#define IORESOURCE_UNSET 0x20000000

#define IORESOURCE_TYPE_BITS 0x00001f00 /* Resource type */

struct resource
{
	resource_size_t start;
	resource_size_t end;
	const char     *name;
	unsigned long   flags;
};

/* helpers to define resources */
#define DEFINE_RES_NAMED(_start, _size, _name, _flags) \
{ \
	.start = (_start), \
	.end = (_start) + (_size) - 1, \
	.name = (_name), \
	.flags = (_flags), \
}

#define DEFINE_RES_MEM_NAMED(_start, _size, _name) \
	DEFINE_RES_NAMED((_start), (_size), (_name), IORESOURCE_MEM)
#define DEFINE_RES_MEM(_start, _size) \
	DEFINE_RES_MEM_NAMED((_start), (_size), NULL)

struct device;

struct resource *request_region(resource_size_t start, resource_size_t n,
                                const char *name);
struct resource *request_mem_region(resource_size_t start, resource_size_t n,
                                    const char *name);
struct resource * devm_request_mem_region(struct device *dev, resource_size_t start,
                                          resource_size_t n, const char *name);


void release_region(resource_size_t start, resource_size_t n);
void release_mem_region(resource_size_t start, resource_size_t n);

static inline resource_size_t resource_size(const struct resource *res)
{
	return res->end - res->start + 1;
}

static inline unsigned long resource_type(const struct resource *res)
{
	return res->flags & IORESOURCE_TYPE_BITS;
}

/* True iff r1 completely contains r2 */
static inline bool resource_contains(struct resource *r1, struct resource *r2)
{
	if (resource_type(r1) != resource_type(r2))
		return false;
	if (r1->flags & IORESOURCE_UNSET || r2->flags & IORESOURCE_UNSET)
		return false;
	return r1->start <= r2->start && r1->end >= r2->end;
}

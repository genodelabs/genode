/*
 * \brief  Linux platform_device emulation
 * \author Sebastian Sumpf
 * \date   2012-06-18
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <lx_emul.h>

#define to_platform_driver(drv) (container_of((drv), struct platform_driver, \
                                 driver))

static int platform_match(struct device *dev, struct device_driver *drv)
{
	if (!dev->name)
		return 0;


	printk("MATCH %s %s\n", dev->name, drv->name);
	return (strcmp(dev->name, drv->name) == 0);
}


static int platform_drv_probe(struct device *_dev)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);

	return drv->probe(dev);
}


struct bus_type platform_bus_type = {
	.name  = "platform",
	.match = platform_match,
	.probe = platform_drv_probe
};


int platform_driver_register(struct platform_driver *drv)
{
	drv->driver.bus = &platform_bus_type;
	if (drv->probe)
		drv->driver.probe = platform_drv_probe;

	printk("Register: %s\n", drv->driver.name);
	return driver_register(&drv->driver);
}


struct resource *platform_get_resource(struct platform_device *dev,
                                       unsigned int type, unsigned int num)
{
	int i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

		if ((type & r->flags) && num-- == 0)
			return r;
	}

	return NULL;
}


struct resource *platform_get_resource_byname(struct platform_device *dev,
                                              unsigned int type,
                                              const char *name)
{
	int i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

	if (type == r->flags && !strcmp(r->name, name))
		return r;
	}

	return NULL;
}


int platform_get_irq_byname(struct platform_device *dev, const char *name)
{
	struct resource *r = platform_get_resource_byname(dev, IORESOURCE_IRQ, name);
	return r ? r->start : -1;
}


int platform_get_irq(struct platform_device *dev, unsigned int num)
{
	struct resource *r = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	return r ? r->start : -1;
}


int platform_device_register(struct platform_device *pdev)
{
	pdev->dev.bus  = &platform_bus_type;
	pdev->dev.name = pdev->name;
	/*Set parent to ourselfs */
	if (!pdev->dev.parent)
		pdev->dev.parent = &pdev->dev;
	device_add(&pdev->dev);
	return 0;
}


struct platform_device *platform_device_alloc(const char *name, int id)
{
	struct platform_device *pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);

	if (!pdev)
		return 0;

	int len    = strlen(name);
	pdev->name = kzalloc(len + 1, GFP_KERNEL);

	if (!pdev->name) {
		kfree(pdev);
		return 0;
	}

	memcpy(pdev->name, name, len);
	pdev->name[len] = 0;
	pdev->id = id;

	return pdev;
}


int platform_device_add_data(struct platform_device *pdev, const void *data,
                             size_t size)
{
	void *d = NULL;

	if (data && !(d = kmemdup(data, size, GFP_KERNEL)))
		return -ENOMEM;

	kfree(pdev->dev.platform_data);
	pdev->dev.platform_data = d;

	return 0;
}


int platform_device_add(struct platform_device *pdev)
{
	return platform_device_register(pdev);
}

int platform_device_add_resources(struct platform_device *pdev,
                                  const struct resource *res, unsigned int num)
{
	struct resource *r = NULL;
	
	if (res) {
		r = kmemdup(res, sizeof(struct resource) * num, GFP_KERNEL);
		if (!r)
			return -ENOMEM;
	}

	kfree(pdev->resource);
	pdev->resource = r;
	pdev->num_resources = num;
	return 0;
}


void *platform_get_drvdata(const struct platform_device *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}


void platform_set_drvdata(struct platform_device *pdev, void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}


/**********************
 ** asm-generic/io.h **
 **********************/

void *_ioremap(resource_size_t phys_addr, unsigned long size, int wc)
{
	dde_kit_addr_t map_addr;
	if (dde_kit_request_mem(phys_addr, size, wc, &map_addr)) {
		panic("Failed to request I/O memory: [%zx,%lx)", phys_addr, phys_addr + size);
		return 0;
	}
	return (void *)map_addr;
}


void *ioremap(resource_size_t offset, unsigned long size)
{
	return _ioremap(offset, size, 0);
}


void *devm_ioremap(struct device *dev, resource_size_t offset,
                   unsigned long size)
{
	return _ioremap(offset, size, 0);
}


void *devm_ioremap_nocache(struct device *dev, resource_size_t offset,
                           unsigned long size)
{
	return _ioremap(offset, size, 0);
}


void *devm_ioremap_resource(struct device *dev, struct resource *res)
{
	return _ioremap(res->start, res->end - res->start, 0);
}

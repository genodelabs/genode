/*
 * \brief  Linux platform_device emulation
 * \author Sebastian Sumpf
 * \date   2012-06-18
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <lx_emul.h>

#define to_platform_driver(drv) (container_of((drv), struct platform_driver, \
                                 driver))
#define to_platform_device(x) container_of((x), struct platform_device, dev)


static int platform_match(struct device *dev, struct device_driver *drv)
{
	if (!dev->name)
		return 0;

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

	return driver_register(&drv->driver);
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


int platform_device_register(struct platform_device *pdev)
{
	pdev->dev.bus  = &platform_bus_type;
	pdev->dev.name = pdev->name;
	/* XXX: Fill with magic value to see page fault */
	pdev->dev.parent = (struct device *)0xaaaaaaaa;
	device_add(&pdev->dev);
	return 0;
}

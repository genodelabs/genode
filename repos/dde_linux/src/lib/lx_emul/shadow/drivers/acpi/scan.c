/*
 * \brief  Replaces driver/acpi/scan.c
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
#include <lx_emul/acpi.h>

#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <acpi/acpi_bus.h>


void acpi_dev_clear_dependencies(struct acpi_device * supplier)
{
	lx_emul_trace(__func__);
}


int acpi_reconfig_notifier_register(struct notifier_block * nb)
{
	lx_emul_trace(__func__);
	return 0;
}


int acpi_reconfig_notifier_unregister(struct notifier_block *nb)
{
	lx_emul_trace(__func__);
	return 0;
}


enum dev_dma_attr acpi_get_dma_attr(struct acpi_device *adev)
{
	if (!adev)
		return DEV_DMA_NOT_SUPPORTED;

#ifdef CONFIG_X86
	/*
	 * ACPI Spec 6.2.17: On Intel platforms, if the _CCA object is not
	 * supplied, the OSPM will assume the devices are hardware cache coherent.
	 *
	 * I did not see any _CCA in the tables relevant currently.
	 */
	return DEV_DMA_COHERENT;
#else
	lx_emul_trace_and_stop(__func__);
#endif
}


int acpi_dma_configure_id(struct device * dev,enum dev_dma_attr attr,const u32 * input_id)
{
	lx_emul_trace(__func__);
	return 0;
}


extern struct acpi_device *acpi_root;


static void acpi_device_del(struct acpi_device *device)
{
	acpi_handle handle;

	if (device->remove)
		device->remove(device);

	device_del(&device->dev);

	handle = device->handle;
	if (handle)
		lx_emul_acpi_set_device(handle, NULL);
}


static void acpi_add_id(struct acpi_device_pnp *pnp, const char *dev_id)
{
	struct acpi_hardware_id *id;

	id = kmalloc(sizeof(*id), GFP_KERNEL);
	if (!id)
		return;

	id->id = kstrdup_const(dev_id, GFP_KERNEL);
	if (!id->id) {
		kfree(id);
		return;
	}

	list_add_tail(&id->list, &pnp->ids);
	pnp->type.hardware_id = 1;
}


static int __acpi_device_add(struct acpi_device *device, char const *name)
{
	int result;

	INIT_LIST_HEAD(&device->wakeup_list);
	INIT_LIST_HEAD(&device->physical_node_list);
	INIT_LIST_HEAD(&device->del_list);

	device->pnp.instance_no = 0;
	dev_set_name(&device->dev, "%s:%02x", name, 0);

	device->dev.bus = &acpi_bus_type;

	result = device_add(&device->dev);
	if (result) {
		dev_err(&device->dev, "Error registering device\n");
		goto err;
	}

	return 0;

err:
	list_del(&device->wakeup_list);

	acpi_device_del(device);

	return result;
}


static struct acpi_device * acpi_add_single_object(acpi_handle handle, char const *name)
{
	int result;
	struct acpi_device *device;

	device = kzalloc(sizeof(struct acpi_device), GFP_KERNEL);
	if (!device)
		return NULL;

	INIT_LIST_HEAD(&device->pnp.ids);
	device->device_type = ACPI_BUS_TYPE_DEVICE;
	device->handle = handle;
	fwnode_init(&device->fwnode, &acpi_device_fwnode_ops);
	strcpy(device->pnp.bus_id, name);
	acpi_add_id(&device->pnp, name);
	if (!strcmp(name, "EPTP"))
		acpi_add_id(&device->pnp, "PNP0C50");
	if (handle != ACPI_ROOT_OBJECT)
		device->pnp.type.platform_id = 1;

	device->flags.match_driver = true;
	device->flags.initialized = true;
	device->flags.enumeration_by_parent = false;
	device_initialize(&device->dev);

	result = __acpi_device_add(device, name);
	if (result)
		return NULL;

	return device;
}


/* FIXME declare API and use it here and elsewhere */
extern struct irq_chip dde_irqchip_data_chip;


static void prepare_acpi_device_irq(unsigned irq)
{
	struct irq_data *irq_data;

	/*
	 * Be lazy and treat irq as hwirq as this is used by the
	 * dde_irqchip_data_chip for (un-)masking.
	 */
	irq_data = irq_get_irq_data(irq);
	irq_data->hwirq = irq;

	irq_set_chip_and_handler(irq, &dde_irqchip_data_chip, handle_level_irq);
}


static unsigned long resource_flags(enum lx_emul_acpi_resource_type type)
{
	switch (type) {
	case LX_EMUL_ACPI_IRQ:    return IORESOURCE_IRQ;
	case LX_EMUL_ACPI_IOPORT: return IORESOURCE_IO;
	case LX_EMUL_ACPI_IOMEM:  return IORESOURCE_MEM;
	}

	return 0;
}



static struct platform_device *create_platform_device(struct acpi_device *adev,
                                                      unsigned res_array_elems,
                                                      struct lx_emul_acpi_resource *res_array)
{
	struct platform_device *pdev = NULL;
	struct platform_device_info pdevinfo;
	struct resource *resources = NULL;

	if (res_array_elems) {
		int i;

		resources = kcalloc(res_array_elems, sizeof(struct resource), GFP_KERNEL);
		if (!resources) {
			dev_err(&adev->dev, "No memory for resources\n");
			return ERR_PTR(-ENOMEM);
		}

		for (i = 0; i < res_array_elems; i++) {
			struct lx_emul_acpi_resource *r = &res_array[i];

			resources[i].start = r->base;
			resources[i].end   = r->base + (r->size - 1);
			resources[i].flags = resource_flags(r->type);

			if (r->type == LX_EMUL_ACPI_IRQ)
				prepare_acpi_device_irq(r->base);
		}
	}

	memset(&pdevinfo, 0, sizeof(pdevinfo));

	pdevinfo.parent   = NULL;
	pdevinfo.name     = dev_name(&adev->dev);
	pdevinfo.id       = -1;
	pdevinfo.res      = resources;
	pdevinfo.num_res  = res_array_elems;
	pdevinfo.fwnode   = acpi_fwnode_handle(adev);
	pdevinfo.dma_mask = DMA_BIT_MASK(32);

	pdev = platform_device_register_full(&pdevinfo);
	if (IS_ERR(pdev))
		dev_err(&adev->dev, "platform device creation failed: %ld\n",
			PTR_ERR(pdev));
	else {
		set_dev_node(&pdev->dev, acpi_get_node(adev->handle));
		dev_dbg(&adev->dev, "created platform device %s\n",
			dev_name(&pdev->dev));
	}

	kfree(resources);

	return pdev;
}


static void * add_lx_emul_device(void *handle, char const *name,
                                 unsigned res_array_elems,
                                 struct lx_emul_acpi_resource *res_array,
                                 void *context)
{
	struct acpi_device *device = NULL;

	device = acpi_add_single_object(handle, name);

	if (!device) {
		printk("%s: acpi_add_single_object(%s) failed\n", __func__, name);
		return NULL;
	}

	create_platform_device(device, res_array_elems, res_array);

	return device;
}


static void create_acpi_root(void)
{
	acpi_root = acpi_add_single_object(ACPI_ROOT_OBJECT, ACPI_SYSTEM_HID);
	create_platform_device(acpi_root, 0, NULL);
	acpi_device_set_enumerated(acpi_root);
}


int __init acpi_scan_init(void)
{
	/* explicitly add ACPI bus root */
	create_acpi_root();

	/* initialize all ACPI devices as platform devices */
	lx_emul_acpi_for_each_device(add_lx_emul_device, NULL);

	return 0;
}

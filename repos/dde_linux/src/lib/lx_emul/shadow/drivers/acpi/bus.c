/*
 * \brief  Replaces driver/acpi/bus.c
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
#include <acpi/acpi_bus.h>


int acpi_bus_attach_private_data(acpi_handle handle, void * data)
{
	lx_emul_trace_and_stop(__func__);
}


void acpi_bus_detach_private_data(acpi_handle handle)
{
	lx_emul_trace_and_stop(__func__);
}


int acpi_bus_get_private_data(acpi_handle handle,void ** data)
{
	lx_emul_trace_and_stop(__func__);
}


/* drivers/acpi/internal.h */
#define ACPI_STA_DEFAULT \
	(ACPI_STA_DEVICE_PRESENT | ACPI_STA_DEVICE_ENABLED | ACPI_STA_DEVICE_UI | ACPI_STA_DEVICE_FUNCTIONING)

int acpi_bus_get_status(struct acpi_device *device)
{
	if (!device)
		return -ENODEV;

	acpi_set_device_status(device, ACPI_STA_DEFAULT);
	return 0;
}


void acpi_set_modalias(struct acpi_device * adev,const char * default_id,char * modalias,size_t len)
{
	lx_emul_trace_and_stop(__func__);
}


static bool __acpi_match_device(struct acpi_device *device,
                                const struct acpi_device_id *acpi_ids,
                                const struct acpi_device_id **out_id)
{
	const struct acpi_device_id *id;
	struct acpi_hardware_id *hwid;

	if (!device || !acpi_ids)
		return false;

	list_for_each_entry(hwid, &device->pnp.ids, list) {
		for (id = acpi_ids; id->id[0] || id->cls; id++) {
			if (id->id[0] && !strcmp((char *)id->id, hwid->id))
				goto out_acpi_match;
		}
	}
	return false;

out_acpi_match:
	if (out_id)
		*out_id = id;
	return true;
}


int acpi_match_device_ids(struct acpi_device *device,const struct acpi_device_id *ids)
{
	return __acpi_match_device(device, ids, NULL) ? 0 : -ENOENT;
}


bool acpi_driver_match_device(struct device *dev, const struct device_driver *drv)
{
	return __acpi_match_device(ACPI_COMPANION(dev), drv->acpi_match_table, NULL);
}


const struct acpi_device_id *acpi_match_device(const struct acpi_device_id *ids,
                                               const struct device *dev)
{
	const struct acpi_device_id *id = NULL;

	if (!is_acpi_device_node(dev->fwnode))
		return NULL;

	__acpi_match_device(ACPI_COMPANION(dev), ids, &id);
	return id;
}


const void *acpi_device_get_match_data(const struct device *dev)
{
	const struct acpi_device_id *match;

	if (!dev->driver->acpi_match_table)
		return NULL;

	if (!is_acpi_device_node(dev->fwnode))
		return NULL;

	match = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!match)
		return NULL;

	return (const void *)match->driver_data;
}


struct acpi_device *acpi_root;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,10,0)
static int acpi_bus_match(struct device *dev, struct device_driver const *drv)
#else
static int acpi_bus_match(struct device *dev, struct device_driver *drv)
#endif
{
	struct acpi_device *acpi_dev = to_acpi_device(dev);
	struct acpi_driver *acpi_drv = to_acpi_driver(drv);

	return acpi_dev->flags.match_driver
		&& !acpi_match_device_ids(acpi_dev, acpi_drv->ids);
}


static int acpi_device_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	lx_emul_trace(__func__);
	return 0;
}


static int acpi_device_probe(struct device *dev)
{
	struct acpi_device *acpi_dev = to_acpi_device(dev);
	struct acpi_driver *acpi_drv = to_acpi_driver(dev->driver);
	int ret;

	if (!acpi_drv->ops.add)
		return -ENOSYS;

	ret = acpi_drv->ops.add(acpi_dev);
	if (ret)
		return ret;

	pr_debug("Driver [%s] successfully bound to device [%s]\n",
		 acpi_drv->name, acpi_dev->pnp.bus_id);

	pr_debug("Found driver [%s] for device [%s]\n", acpi_drv->name,
		 acpi_dev->pnp.bus_id);

	get_device(dev);
	return 0;
}


static void acpi_device_remove(struct device *dev)
{
	struct acpi_device *acpi_dev = to_acpi_device(dev);
	struct acpi_driver *acpi_drv = to_acpi_driver(dev->driver);

	if (acpi_drv) {
		if (acpi_drv->ops.remove)
			acpi_drv->ops.remove(acpi_dev);
	}
	acpi_dev->driver_data = NULL;

	put_device(dev);
}


struct bus_type const acpi_bus_type = {
	.name		= "acpi",
	.match		= acpi_bus_match,
	.probe		= acpi_device_probe,
	.remove		= acpi_device_remove,
	.uevent		= acpi_device_uevent,
};


static int __init acpi_bus_init(void)
{
	int result;

	result = bus_register(&acpi_bus_type);
	if (!result)
		return 0;

	return -ENODEV;
}


struct kobject *acpi_kobj;


/* drivers/acpi/internal.h */
extern void acpi_scan_init(void);

static int __init acpi_init(void)
{
	int result;

	acpi_kobj = kobject_create_and_add("acpi", firmware_kobj);
	if (!acpi_kobj)
		pr_debug("%s: kset create error\n", __func__);

	result = acpi_bus_init();
	if (result) {
		kobject_put(acpi_kobj);
		return result;
	}

	lx_emul_acpi_init();

	acpi_scan_init();

	return 0;
}

subsys_initcall(acpi_init);

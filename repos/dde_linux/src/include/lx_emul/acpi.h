/*
 * \brief  Lx_emul support for ACPI devices
 * \author Christian Helmuth
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__ACPI_H_
#define _LX_EMUL__ACPI_H_

#ifdef __cplusplus
extern "C" {
#endif

enum lx_emul_acpi_resource_type { LX_EMUL_ACPI_IRQ, LX_EMUL_ACPI_IOPORT, LX_EMUL_ACPI_IOMEM };

struct lx_emul_acpi_resource {
	enum lx_emul_acpi_resource_type type;
	unsigned long                   base;
	unsigned long                   size;
};

extern char const * lx_emul_acpi_name(void *handle);
extern void * lx_emul_acpi_device(void *handle);
extern void * lx_emul_acpi_object(void *handle);

extern void lx_emul_acpi_set_device(void *handle, void *device);
extern void lx_emul_acpi_set_object(void *handle, void *object);

/**
 * Called for each ACPI device in lx_emul_acpi_for_each_device()
 *
 * \param handle         opaque handle for the ACPI device
 * \param name           device name
 * \param num_resources  number of resources in array
 * \param resources      array of device resources
 * \param context        opaque context pointer
 *
 * \return opaque device pointer to be stored with the ACPI device that can be
 *         retrieved with lx_emul_acpi_device()
 *
 * If the function returns NULL, device data in the device is not updated. So,
 * lx_emul_acpi_for_each_device() can be called without changing the database.
 */
typedef void * (*lx_emul_acpi_device_callback)
	(void *handle, char const *name, unsigned num_resources,
	 struct lx_emul_acpi_resource *resources, void *context);

extern void lx_emul_acpi_for_each_device(lx_emul_acpi_device_callback cb, void *context);

extern void lx_emul_acpi_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__USB_H_ */

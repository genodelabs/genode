/*
 * \brief  Linux emulation environment for firmware loading
 * \author Josef Soentgen
 * \date   2026-03-04
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

/* lx emul/kit includes */
#include <lx_emul.h>

/* Linux includes */
#include <linux/version.h>
#include <linux/firmware.h>


struct firmware_work {
	struct work_struct work;
	struct firmware *firmware;
	char const *name;
	void *context;
	void (*cont)(struct firmware const *, void *);
};


extern int lx_emul_request_firmware_nowait(const char *name, void *dest,
                                           size_t *result, bool warn);
extern void lx_emul_release_firmware(void const *data, size_t size);


static void request_firmware_work_func(struct work_struct *work)
{
	struct firmware_work *fw_work =
		container_of(work, struct firmware_work, work);
	struct firmware *fw = fw_work->firmware;

	if (lx_emul_request_firmware_nowait(fw_work->name,
	                                    &fw->data, &fw->size, true)) {
		/*
		 * Free and set to NULL here as passing NULL to
		 * 'cont()' triggers requesting next possible ucode
		 * version.
		 */
		kfree(fw);
		fw = NULL;
	}

	fw_work->cont(fw, fw_work->context);

	kfree(fw_work);
}


int request_firmware_nowait(struct module *module,
                            bool uevent, const char *name,
                            struct device *device, gfp_t gfp,
                            void *context,
                            void (*cont)(const struct firmware *fw,
                                         void *context))
{
	struct firmware *fw = kzalloc(sizeof (struct firmware), GFP_KERNEL);
	struct firmware_work *fw_work;

	fw_work = kzalloc(sizeof (struct firmware_work), GFP_KERNEL);
	if (!fw_work) {
		kfree(fw);
		return -1;
	}

	fw_work->name     = name;
	fw_work->firmware = fw;
	fw_work->context  = context;
	fw_work->cont     = cont;

	INIT_WORK(&fw_work->work, request_firmware_work_func);
	schedule_work(&fw_work->work);

	return 0;
}


int request_firmware_common(const struct firmware **firmware_p,
                            const char *name, struct device *device,
                            bool warn)
{
	struct firmware *fw;

	if (!firmware_p)
		return -1;

	fw = kzalloc(sizeof(struct firmware), GFP_KERNEL);

	if (lx_emul_request_firmware_nowait(name, &fw->data, &fw->size, warn)) {
		kfree(fw);
		return -1;
	}

	*firmware_p = fw;
	return 0;
}


int request_firmware(const struct firmware **firmware_p,
                     const char *name, struct device *device)
{
	return request_firmware_common(firmware_p, name, device, true);
}


void release_firmware(const struct firmware * fw)
{
	if (!fw)
		return;

	lx_emul_release_firmware(fw->data, fw->size);
	kfree(fw);
}


int firmware_request_nowarn(const struct firmware **firmware,
                            const char *name, struct device *device)
{
	return request_firmware_common(firmware, name, device, false);
}

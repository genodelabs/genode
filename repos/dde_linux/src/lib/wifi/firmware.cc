/*
 * \brief  Linux wireless stack
 * \author Josef Soentgen
 * \date   2018-06-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <lx_kit/env.h>
#include <firmware_list.h>
#include <lx_emul.h>


Firmware_list fw_list[] = {
	{ "regulatory.db", 4144, nullptr },

	{ "iwlwifi-1000-5.ucode",     337520, nullptr },
	{ "iwlwifi-3160-17.ucode",    918268, nullptr },
	{ "iwlwifi-5000-5.ucode",     340696, nullptr },
	{ "iwlwifi-6000-4.ucode",     454608, nullptr },
	{ "iwlwifi-6000-6.ucode",     454608, "iwlwifi-6000-4.ucode" },
	{ "iwlwifi-6000g2a-6.ucode",  677296, nullptr },
	{ "iwlwifi-6000g2b-6.ucode",  679436, nullptr },
	{ "iwlwifi-7260-17.ucode",   1049340, nullptr },
	{ "iwlwifi-7265-16.ucode",   1180412, nullptr },
	{ "iwlwifi-7265D-22.ucode",  1028376, nullptr },
	{ "iwlwifi-7265D-29.ucode",  1036432, nullptr },
	{ "iwlwifi-8000C-22.ucode",  2120860, nullptr },
	{ "iwlwifi-8000C-36.ucode",  2486572, nullptr },
	{ "iwlwifi-8265-22.ucode",   1811984, nullptr },
	{ "iwlwifi-8265-36.ucode",   2498044, nullptr }
};


size_t fw_list_len = sizeof(fw_list) / sizeof(fw_list[0]);


/**********************
 ** linux/firmware.h **
 **********************/

int request_firmware_nowait(struct module *module, bool uevent,
                            const char *name, struct device *device,
                            gfp_t gfp, void *context,
                            void (*cont)(const struct firmware *, void *))
{
	/* only try to load known firmware images */
	Firmware_list *fwl = 0;
	for (size_t i = 0; i < fw_list_len; i++) {
		if (Genode::strcmp(name, fw_list[i].requested_name) == 0) {
			fwl = &fw_list[i];
			break;
		}
	}

	if (!fwl) {
		Genode::error("firmware '", name, "' is not in the firmware white list");
		return -1;
	}

	char const *fw_name = fwl->available_name
	                    ? fwl->available_name : fwl->requested_name;
	Genode::Rom_connection rom(Lx_kit::env().env(), fw_name);
	Genode::Dataspace_capability ds_cap = rom.dataspace();

	if (!ds_cap.valid()) {
		Genode::error("could not get firmware ROM dataspace");
		return -1;
	}

	struct firmware *fw = (struct firmware *)kzalloc(sizeof(struct firmware), 0);
	if (!fw) {
		Genode::error("could not allocate memory for firmware metadata");
		return -1;
	}

	/* use allocator because fw is too big for slab */
	if (!Lx_kit::env().heap().alloc(fwl->size, (void**)&fw->data)) {
		Genode::error("Could not allocate memory for firmware image");
		kfree(fw);
		return -1;
	}

	void const *image = Lx_kit::env().env().rm().attach(ds_cap);
	Genode::memcpy((void*)fw->data, image, fwl->size);
	Lx_kit::env().env().rm().detach(image);

	fw->size = fwl->size;

	cont(fw, context);

	return 0;
}

void release_firmware(const struct firmware *fw)
{
	Lx_kit::env().heap().free(const_cast<u8 *>(fw->data), fw->size);
	kfree(fw);
}

/*
 * \brief  Linux wireless stack
 * \author Josef Soentgen
 * \date   2018-06-29
 */

/*
 * Copyright (C) 2018-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <rom_session/connection.h>

/* local includes */
#include <lx_kit/env.h>
#include <firmware_list.h>


Firmware_list fw_list[] = {
	{ "regulatory.db",     4144, nullptr },
	{ "regulatory.db.p7s", 1182, nullptr },

	{ "iwlwifi-1000-5.ucode",     337520, nullptr },
	{ "iwlwifi-3160-17.ucode",    918268, nullptr },
	{ "iwlwifi-5000-5.ucode",     340696, nullptr },
	{ "iwlwifi-6000-4.ucode",     454608, nullptr },
	{ "iwlwifi-6000-6.ucode",     454608, "iwlwifi-6000-4.ucode" },
	{ "iwlwifi-6000g2a-6.ucode",  677296, nullptr },
	{ "iwlwifi-6000g2b-6.ucode",  679436, nullptr },
	{ "iwlwifi-7260-17.ucode",   1049340, nullptr },
	{ "iwlwifi-7265-16.ucode",   1180412, nullptr },
	{ "iwlwifi-7265D-29.ucode",  1036772, nullptr },
	{ "iwlwifi-8000C-22.ucode",  2120860, nullptr },
	{ "iwlwifi-8000C-36.ucode",  2428004, nullptr },
	{ "iwlwifi-8265-22.ucode",   1811984, nullptr },
	{ "iwlwifi-8265-36.ucode",   2436632, nullptr },

	{ "iwlwifi-9000-pu-b0-jf-b0-34.ucode", 2678284, nullptr },
	{ "iwlwifi-9000-pu-b0-jf-b0-36.ucode", 2678284, "iwlwifi-9000-pu-b0-jf-b0-34.ucode" },
	{ "iwlwifi-9000-pu-b0-jf-b0-46.ucode", 1514876, nullptr },

	{ "iwlwifi-QuZ-a0-hr-b0-63.ucode", 1334804, nullptr },
	{ "iwlwifi-QuZ-a0-hr-b0-64.ucode", 1334804, "iwlwifi-QuZ-a0-hr-b0-63.ucode" },
	{ "iwlwifi-so-a0-hr-b0-64.ucode",  1427384, nullptr },
	{ "iwlwifi-so-a0-gf-a0-64.ucode",  1515812, nullptr },
	{ "iwlwifi-so-a0-gf-a0.pnvm", 41808, nullptr },
};


size_t fw_list_len = sizeof(fw_list) / sizeof(fw_list[0]);


/**********************
 ** linux/firmware.h **
 **********************/

extern "C" int lx_emul_request_firmware_nowait(const char *name, void **dest,
                                               size_t *result, bool warn)
{
	if (!dest || !result)
		return -1;

	/* only try to load known firmware images */
	Firmware_list *fwl = 0;
	for (size_t i = 0; i < fw_list_len; i++) {
		if (Genode::strcmp(name, fw_list[i].requested_name) == 0) {
			fwl = &fw_list[i];
			break;
		}
	}

	if (!fwl ) {
		if (warn)
			Genode::error("firmware '", name, "' is not in the firmware white list");

		return -1;
	}

	char const *fw_name = fwl->available_name
	                    ? fwl->available_name : fwl->requested_name;
	Genode::Rom_connection rom(Lx_kit::env().env, fw_name);
	Genode::Dataspace_capability ds_cap = rom.dataspace();

	if (!ds_cap.valid()) {
		Genode::error("could not get firmware ROM dataspace");
		return -1;
	}

	/* use allocator because fw is too big for slab */
	void *data = Lx_kit::env().heap.alloc(fwl->size);
	if (!data)
		return -1;

	void const *image = Lx_kit::env().env.rm().attach(ds_cap);
	Genode::memcpy(data, image, fwl->size);
	Lx_kit::env().env.rm().detach(image);

	*dest   = data;
	*result = fwl->size;

	return 0;
}


extern "C" void lx_emul_release_firmware(void const *data, size_t size)
{
	Lx_kit::env().heap.free(const_cast<void *>(data), size);
}

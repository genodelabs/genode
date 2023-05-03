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


using namespace Genode;

/**********************
 ** linux/firmware.h **
 **********************/

extern size_t _wifi_probe_firmware(char const *name);
extern int    _wifi_request_firmware(char const *name, char *dst, size_t dst_len);

extern "C" int lx_emul_request_firmware_nowait(const char *name, void **dest,
                                               size_t *result, bool /* warn */)
{
	if (!dest || !result)
		return -1;

	size_t const fw_size = _wifi_probe_firmware(name);

	if (!fw_size)
		return -1;

	/* use allocator because fw is too big for slab */
	char *data = (char*)Lx_kit::env().heap.alloc(fw_size);
	if (!data)
		return -1;

	if (_wifi_request_firmware(name, data, fw_size)) {
		error("could not request firmware ", name);
		Lx_kit::env().heap.free(data, fw_size);
		return -1;
	}

	*dest   = data;
	*result = fw_size;

	return 0;
}


extern "C" void lx_emul_release_firmware(void const *data, size_t size)
{
	Lx_kit::env().heap.free(const_cast<void *>(data), size);
}

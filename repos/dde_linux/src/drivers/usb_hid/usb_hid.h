/*
 * \brief  C/C++ interface for this driver
 * \author Sebastian Sumpf
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <genode_c_api/usb_client.h>
#include <lx_emul/usb_client.h>

#ifdef __cplusplus
extern "C" {
#endif

void lx_emul_led_state_update(void);

#ifdef __cplusplus
} /* extern "C" */
#endif



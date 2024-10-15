/*
 * \brief  Lx_emul support for NICs
 * \author Stefan Kalkowski
 * \date   2024-10-15
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__NIC_H_
#define _LX_EMUL__NIC_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void lx_emul_nic_init(void);
extern void lx_emul_nic_handle_io(void);
extern void lx_emul_nic_set_mac_address(const unsigned char *mac,
                                        unsigned long size);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__USB_H_ */


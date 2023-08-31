/*
 * \brief  C back-end
 * \author Sebastian Sumpf
 * \date   2013-09-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _INCLUDE__NIC_H_
#define _INCLUDE__NIC_H_

#ifdef __cplusplus
extern "C" {
#endif

void net_mac(void* mac, unsigned long size);
int  net_tx(void* addr, unsigned long len);
void net_driver_rx(void *addr, unsigned long size);

#ifdef __cplusplus
}
#endif
#endif /* _INCLUDE__NIC_H_ */

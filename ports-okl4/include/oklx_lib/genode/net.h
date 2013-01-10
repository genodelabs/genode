/*
 * \brief  Genode C API network functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-09-10
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLX_LIB__GENODE__NET_H_
#define _INCLUDE__OKLX_LIB__GENODE__NET_H_

int   genode_net_ready       (void);
void  genode_net_start       (void *dev, void (*func)(void*, void*, unsigned long));
void  genode_net_stop        (void);
void  genode_net_mac         (void* mac_addr, unsigned long size);
int   genode_net_tx          (void* addr,     unsigned long len, void *skb);
int   genode_net_tx_ack_avail(void);
void* genode_net_tx_ack      (void);
void  genode_net_rx_receive  (void);

#endif /* _INCLUDE__OKLX_LIB__GENODE__NET_H_ */

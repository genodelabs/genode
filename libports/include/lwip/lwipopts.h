/*
 * \brief  Configuration file for LwIP, adapt it to your needs.
 * \author Stefan Kalkowski
 * \date   2009-11-10
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef __LWIP__LWIPOPTS_H__
#define __LWIP__LWIPOPTS_H__

#include <stdlib.h>
#include <string.h>

#define NO_SYS                      0  /* single-threaded? do not touch! */
#define SYS_LIGHTWEIGHT_PROT        1  /* do we provide lightweight protection */
#define LWIP_ARP                    1  /* ARP support */
#define LWIP_RAW                    1  /* LwIP raw API */
#define LWIP_UDP                    1  /* UDP support */
#define LWIP_TCP                    1  /* TCP support */
#define LWIP_DNS                    1  /* DNS support */
#define LWIP_DHCP                   1  /* DHCP support */
#define LWIP_SOCKET                 1  /* LwIP socket API */
#define LWIP_COMPAT_SOCKETS         0  /* Libc compatibility layer */
#define LWIP_COMPAT_MUTEX           1  /* use binary semaphore instead of mutex */
#define LWIP_NETIF_API              1  /* Network interface API */
#define LWIP_NETIF_LOOPBACK         1  /* Looping back to same address? */
#define LWIP_HAVE_LOOPIF            1  /* 127.0.0.1 support ? */

#if LWIP_DHCP
#define LWIP_NETIF_STATUS_CALLBACK  1  /* callback function used by DHCP init */
#endif


/*********************
 ** Memory settings **
 *********************/

#define MEM_LIBC_MALLOC             1
#define MEMP_MEM_MALLOC             1
#define DEFAULT_ACCEPTMBOX_SIZE   128
#define TCPIP_MBOX_SIZE           128

#define TCP_MSS                  1460
#define TCP_SND_BUF     (2 * TCP_MSS)

/*
 * We reduce the maximum segment lifetime from one minute to one second to
 * avoid queuing up PCBs in TIME-WAIT state. This is the state, PCBs end up
 * after closing a TCP connection socket at the server side. The number of PCBs
 * in this state is apparently not limited by the value of 'MEMP_NUM_TCP_PCB'.
 * One allocation costs around 160 bytes. If clients connect to the server at a
 * high rate, those allocations accumulate quickly and thereby may exhaust the
 * memory of the server. By reducing the segment lifetime, PCBs in TIME-WAIT
 * state are cleaned up from the 'tcp_tw_pcbs' queue in a more timely fashion
 * (by 'tcp_slowtmr()').
 */
#define TCP_MSL 1000UL

#define MEMP_NUM_SYS_TIMEOUT        8
#define MEMP_NUM_TCP_PCB           64
#define MEMP_NUM_NETCONN (MEMP_NUM_TCP_PCB + MEMP_NUM_UDP_PCB + MEMP_NUM_RAW_PCB + MEMP_NUM_TCP_PCB_LISTEN - 1)

/********************
 ** Debug settings **
 ********************/

/* #define LWIP_DEBUG */
/* #define DHCP_DEBUG      LWIP_DBG_ON */
/* #define ETHARP_DEBUG    LWIP_DBG_ON */
/* #define NETIF_DEBUG     LWIP_DBG_ON */
/* #define PBUF_DEBUG      LWIP_DBG_ON */
/* #define API_LIB_DEBUG   LWIP_DBG_ON */
/* #define API_MSG_DEBUG   LWIP_DBG_ON */
/* #define SOCKETS_DEBUG   LWIP_DBG_ON */
/* #define ICMP_DEBUG      LWIP_DBG_ON */
/* #define INET_DEBUG      LWIP_DBG_ON */
/* #define IP_DEBUG        LWIP_DBG_ON */
/* #define IP_REASS_DEBUG  LWIP_DBG_ON */
/* #define RAW_DEBUG       LWIP_DBG_ON */
/* #define MEM_DEBUG       LWIP_DBG_ON */
/* #define MEMP_DEBUG      LWIP_DBG_ON */
/* #define SYS_DEBUG       LWIP_DBG_ON */
/* #define TCP_DEBUG       LWIP_DBG_ON */

#endif /* __LWIP__LWIPOPTS_H__ */

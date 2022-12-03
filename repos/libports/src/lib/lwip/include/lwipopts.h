/*
 * \brief  Configuration file for LwIP, adapt it to your needs.
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2009-11-10
 *
 * See lwip/src/include/lwip/opt.h for all options
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__LWIPOPTS_H__
#define __LWIP__LWIPOPTS_H__

/* Genode includes */
#include <base/fixed_stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Use lwIP without OS-awareness
 */
#define NO_SYS 1
#define SYS_LIGHTWEIGHT_PROT 0

#define LWIP_DNS                    1  /* DNS support */
#define LWIP_DHCP                   1  /* DHCP support */
#define LWIP_SOCKET                 0  /* LwIP socket API */
#define LWIP_NETIF_LOOPBACK         1  /* Looping back to same address? */
#define LWIP_STATS                  0  /* disable stating */
#define LWIP_TCP_TIMESTAMPS         1
#define TCP_LISTEN_BACKLOG              1
#define TCP_MSS                         1460
#define TCP_WND                     (32 * TCP_MSS)
#define TCP_SND_BUF                 (32 * TCP_MSS)
#define LWIP_WND_SCALE                  3
#define TCP_RCV_SCALE                   2
#define TCP_SND_QUEUELEN                ((8 * (TCP_SND_BUF) + (TCP_MSS - 1))/(TCP_MSS))

#define LWIP_NETIF_STATUS_CALLBACK  1  /* callback function used for interface changes */
#define LWIP_NETIF_LINK_CALLBACK    1  /* callback function used for link-state changes */


/***********************************
 ** Checksum calculation settings **
 ***********************************/

/* checksum calculation for outgoing packets can be disabled if the hardware supports it */
#define LWIP_CHECKSUM_ON_COPY       1  /* calculate checksum during memcpy */

/*********************
 ** Memory settings **
 *********************/

#define MEM_LIBC_MALLOC             1
#define MEMP_MEM_MALLOC             1
/* MEM_ALIGNMENT > 4 e.g. for x86_64 are not supported, see Genode issue #817 */
#define MEM_ALIGNMENT               4

#define DEFAULT_ACCEPTMBOX_SIZE   128
#define TCPIP_MBOX_SIZE           128

#define RECV_BUFSIZE_DEFAULT        (512*1024)

#define PBUF_POOL_SIZE             96

#define MEMP_NUM_SYS_TIMEOUT        16
#define MEMP_NUM_TCP_PCB           128

#ifndef MEMCPY
#define MEMCPY(dst,src,len)             genode_memcpy(dst,src,len)
#endif

#ifndef MEMMOVE
#define MEMMOVE(dst,src,len)            genode_memmove(dst,src,len)
#endif

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


/*
   ----------------------------------
   ---------- DHCP options ----------
   ----------------------------------
*/

#define LWIP_DHCP_CHECK_LINK_UP         1


/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/
/* no Netconn API */
#define LWIP_NETCONN                    0


/*
   ---------------------------------------
   ---------- IPv6 options ---------------
   ---------------------------------------
*/

#define LWIP_IPV6                       1
#define IPV6_FRAG_COPYHEADER            1

#ifdef __cplusplus
}
#endif

#endif /* __LWIP__LWIPOPTS_H__ */

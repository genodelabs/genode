#ifndef _LX_USER_EMUL_H_
#define _LX_USER_EMUL_H_

/**************************
 ** asm-generic/socket.h **
 **************************/

/* enum { */
/* 	SOL_SOCKET = 1, */
/* }; */

enum {
	SO_PRIORITY      = 12,
	SO_ATTACH_FILTER = 26,
};


/*******************
 ** bits/ioctls.h **
 *******************/

enum {
	/* SIOCGIFFLAGS  = 0x8913, */
	/* SIOCSIFFLAGS  = 0x8914, */
	/* SIOCGIFADDR   = 0x8915, */
	SIOCSIFHWADDR = 0x8924,
	SIOCGIFHWADDR = 0x8927,
	/* SIOCGIFINDEX  = 0x8933, */
};


/*******************
 ** bits/socket.h **
 *******************/

enum {
	PF_NETLINK = 16,
	PF_PACKET = 17,
};

enum {
	MSG_ERRQUEUE = 0x2000,
};

enum {
	SOL_PACKET = 263,
};


/******************
 ** bits/types.h **
 ******************/

typedef char *__caddr_t;


/********************
 ** linux/socket.h **
 ********************/

enum {
	AF_BRIDGE = 7,
	AF_NETLINK = 16,
	AF_PACKET = 17,
};


/**********************
 ** linux/errqueue.h **
 **********************/

struct sock_extended_err
{
	unsigned int   ee_errno;
	unsigned char    ee_origin;
	unsigned char    ee_type;
	unsigned char    ee_code;
	unsigned char    ee_pad;
	unsigned int   ee_info;
	unsigned int   ee_data;
};


/********************
 ** linux/filter.h **
 ********************/

enum {
	BPF_LD   = 0x00,
	BPF_ALU  = 0x04,
	BPF_JMP  = 0x05,
	BPF_RET  = 0x06,
	BPF_MISC = 0x07,

	BPF_W    = 0x00,
	BPF_H    = 0x08,
	BPF_B    = 0x10,
	BPF_IND  = 0x40,
	BPF_LEN  = 0x80,

	BPF_ADD  = 0x00,
	BPF_OR   = 0x40,
	BPF_AND  = 0x50,
	BPF_LSH  = 0x60,
	BPF_RSH  = 0x70,
	BPF_JA   = 0x00,
	BPF_JEQ  = 0x10,
	BPF_K    = 0x00,
	BPF_X    = 0x08,

	BPF_ABS = 0x20,

	BPF_TAX  = 0x00,

};

#define BPF_CLASS(code) ((code) & 0x07)

#ifndef BPF_STMT
#define BPF_STMT(code, k) { (unsigned short)(code), 0, 0, k }
#endif
#ifndef BPF_JUMP
#define BPF_JUMP(code, k, jt, jf) { (unsigned short)(code), jt, jf, k }
#endif

struct sock_filter
{
	unsigned short code;
	unsigned char  jt;
	unsigned char  jf;
	unsigned int   k;
};

struct sock_fprog
{
	unsigned short      len;
	struct sock_filter *filter;
};


/***********************
 ** linux/if_packet.h **
 ***********************/

struct sockaddr_ll {
	unsigned short  sll_family;
	unsigned short  sll_protocol; /* BE 16b */
	int             sll_ifindex;
	unsigned short  sll_hatype;
	unsigned char   sll_pkttype;
	unsigned char   sll_halen;
	unsigned char   sll_addr[8];
};


/**************
 ** net/if.h **
 **************/

enum {
	IFF_UP = 0x01,
	IFF_RUNNING = 0x40,
};

struct ifmap
{
	unsigned long int  mem_start;
	unsigned long int  mem_end;
	unsigned short int base_addr;
	unsigned char      irq;
	unsigned char      dma;
	unsigned char      port;
	/* 3 bytes spare */
};

struct ifreq
{
#define IFHWADDRLEN 6
#define IF_NAMESIZE 16
#define IFNAMSIZ    IF_NAMESIZE
	union
	{
		char ifrn_name[IFNAMSIZ];   /* Interface name, e.g. "en0".  */
	} ifr_ifrn;

	union
	{
		struct sockaddr ifru_addr;
		struct sockaddr ifru_dstaddr;
		struct sockaddr ifru_broadaddr;
		struct sockaddr ifru_netmask;
		struct sockaddr ifru_hwaddr;
		short int ifru_flags;
		int ifru_ivalue;
		int ifru_mtu;
		struct ifmap ifru_map;
		char ifru_slave[IFNAMSIZ];  /* Just fits the size */
		char ifru_newname[IFNAMSIZ];
		__caddr_t ifru_data;
	} ifr_ifru;
#define ifr_name      ifr_ifrn.ifrn_name  /* interface name   */
#define ifr_hwaddr    ifr_ifru.ifru_hwaddr    /* MAC address      */
#define ifr_addr      ifr_ifru.ifru_addr  /* address      */
#define ifr_dstaddr   ifr_ifru.ifru_dstaddr   /* other end of p-p lnk */
#define ifr_broadaddr ifr_ifru.ifru_broadaddr /* broadcast address    */
#define ifr_netmask   ifr_ifru.ifru_netmask   /* interface net mask   */
#define ifr_flags     ifr_ifru.ifru_flags /* flags        */
#define ifr_metric    ifr_ifru.ifru_ivalue    /* metric       */
#define ifr_mtu       ifr_ifru.ifru_mtu   /* mtu          */
#define ifr_map       ifr_ifru.ifru_map   /* device map       */
#define ifr_slave     ifr_ifru.ifru_slave /* slave device     */
#define ifr_data      ifr_ifru.ifru_data  /* for use by interface */
#define ifr_ifindex   ifr_ifru.ifru_ivalue    /* interface index  */
#define ifr_bandwidth ifr_ifru.ifru_ivalue    /* link bandwidth   */
#define ifr_qlen      ifr_ifru.ifru_ivalue    /* Queue length     */
#define ifr_newname   ifr_ifru.ifru_newname   /* New name     */
#define ifr_settings  ifr_ifru.ifru_settings  /* Device/proto settings*/
};

unsigned int if_nametoindex(const char *ifname);
char *if_indextoname(unsigned int ifindex, char *ifname);


/******************
 ** net/if_arp.h **
 ******************/

enum {
	ARPHRD_ETHER = 1,
};

#endif /* _LX_USER_EMUL_H_ */

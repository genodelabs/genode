/*
 * \brief  Linux emulation code
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2013-08-30
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux includes */
#include <linux/inetdevice.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <uapi/linux/rtnetlink.h>
#include <net/sock.h>
#include <net/route.h>
#include <net/ip_fib.h>
#include <net/tcp.h>
#include <net/tcp_states.h>


/***********************
 ** linux/genetlink.h **
 ***********************/

/* needed by af_netlink.c */
atomic_t genl_sk_destructing_cnt;
wait_queue_head_t genl_sk_destructing_waitq;


/****************************
 ** asm-generic/atomic64.h **
 ****************************/

long long atomic64_read(const atomic64_t *v)
{
	return v->counter;
}


void atomic64_set(atomic64_t *v, long long i)
{
	v->counter = i;
}


/********************
 ** linux/bitmap.h **
 ********************/

void bitmap_fill(unsigned long *dst, int nbits)
{
	int count = nbits / (8 * sizeof (long));
	int i;

	memset(dst, 0xff, count * sizeof(long));
	dst += count;

	count = nbits % (8 * sizeof(long));

	for (i = 0; i < count; i++)
		*dst |= (1 << i);
}


void bitmap_zero(unsigned long *dst, int nbits)
{
	int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
	memset(dst, 0, len);
}


/*****************
 ** linux/gfp.h **
 *****************/

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return (unsigned long)kzalloc(PAGE_SIZE, 0);
}


/********************
 ** linux/percpu.h **
 ********************/

void *__alloc_percpu(size_t size, size_t align)
{
	return kzalloc(size, 0);
}


/******************
 ** linux/hash.h **
 ******************/

u32 hash_32(u32 val, unsigned int bits)
{
	enum  { GOLDEN_RATIO_PRIME_32 = 0x9e370001UL };
	u32 hash = val * GOLDEN_RATIO_PRIME_32;

	hash =  hash >> (32 - bits);
	return hash;
}


/******************
 ** linux/dcache **
 ******************/

unsigned int full_name_hash(const unsigned char *name, unsigned int len)
{
	unsigned hash = 0, i;
	for (i = 0; i < len; i++)
		hash += name[i];

	return hash;
}


/******************************
 *  net/core/net/namespace.h **
 ******************************/

int register_pernet_subsys(struct pernet_operations *ops)
{
	if (ops->init)
		ops->init(&init_net);
	
	return 0;
}

int register_pernet_device(struct pernet_operations *ops)
{
	return register_pernet_subsys(ops);
}


/*************************
 ** net/net_namespace.h **
 *************************/

int rt_genid(struct net *net)
{
	int ret = atomic_read(&net->rt_genid);
	return -1;
}

int rt_genid_ipv4(struct net *net)
{
    return atomic_read(&net->ipv4.rt_genid);
}

void rt_genid_bump_ipv4(struct net *net)
{
    atomic_inc(&net->ipv4.rt_genid);
}

/**********************
 ** linx/rtnetlink.h **
 **********************/

struct netdev_queue *dev_ingress_queue(struct net_device *dev)
{
	return dev->ingress_queue; 
}

void rtnl_notify(struct sk_buff *skb, struct net *net, u32 pid, u32 group,
                 struct nlmsghdr *nlh, gfp_t flags)
{
	nlmsg_free(skb);
}


/****************
 ** linux/ip.h **
 ****************/

struct iphdr *ip_hdr(const struct sk_buff *skb)
{
	return (struct iphdr *)skb_network_header(skb);
}


/*******************************
 ** asm-generic/bitops/find.h **
 *******************************/

unsigned long find_first_zero_bit(unsigned long const *addr, unsigned long size)
{
	unsigned long i, j;

	for (i = 0; i < (size / BITS_PER_LONG); i++)
		if (addr[i] != ~0UL)
			break;

	if (i == size)
		return size;

	for (j = 0; j < BITS_PER_LONG; j++)
		if ((~addr[i]) & (1 << j))
			break;

	return (i * BITS_PER_LONG) + j;
}


/****************************
 ** asm-generic/getorder.h **
 ****************************/

int get_order(unsigned long size)
{
	int order;

	size--;
	size >>= PAGE_SHIFT;

	order = __builtin_ctzl(size);
	return order;
}


/*********************
 ** linux/jiffies.h **
 *********************/

clock_t jiffies_to_clock_t(unsigned long j)
{
	return j / HZ; /* XXX not sure if this is enough */
}


/*********************
 ** linux/utsname.h **
 *********************/

struct uts_name init_uts_ns;

struct new_utsname *init_utsname(void) { return &init_uts_ns.name; }
struct new_utsname *utsname(void) { return init_utsname(); }


/**********************
 ** linux/notifier.h **
 **********************/

int raw_notifier_chain_register(struct raw_notifier_head *nh,
                                struct notifier_block *n)
{
	struct notifier_block *nl = nh->head;
	struct notifier_block *pr = 0;
	while (nl) {
		if (n->priority > nl->priority)
			break;
		pr = nl;
		nl = nl->next;
	}

	n->next = nl;
	if (pr)
		pr->next = n;
	else
		nh->head = n;

	return 0;
}


int raw_notifier_call_chain(struct raw_notifier_head *nh,
                            unsigned long val, void *v)
{
	int ret = NOTIFY_DONE;
	struct notifier_block *nb = nh->head;

	while (nb) {
		
		ret = nb->notifier_call(nb, val, v);
		if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
			break;

		nb = nb->next;
	}

	return ret;
}


int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                     struct notifier_block *n)
{
	return raw_notifier_chain_register((struct raw_notifier_head *)nh, n);
}


int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long val, void *v)
{
	return raw_notifier_call_chain((struct raw_notifier_head *)nh, val, v);
}


/****************************
 ** asm-generic/checksum.h **
 ****************************/

__sum16 csum_fold(__wsum csum)
{
	u32 sum = (u32)csum;
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return (__sum16)~sum;
}


/*******************
** net/checksum.h **
********************/

__wsum csum_add(__wsum csum, __wsum addend)
{
	u32 res = (u32)csum;
	res += (u32)addend;
	return (__wsum)(res + (res < (u32)addend));
}


__wsum csum_block_add(__wsum csum, __wsum csum2, int offset)
{
	u32 sum = (u32)csum2;
	if (offset&1)
		sum = ((sum&0xFF00FF)<<8)+((sum>>8)&0xFF00FF);
	return csum_add(csum, (__wsum)sum);
}


__wsum csum_block_add_ext(__wsum csum, __wsum csum2, int offset, int len)
{
	return csum_block_add(csum, csum2, offset);
}


/***************************
 ** Linux socket function **
 ***************************/

static const struct net_proto_family *net_families[NPROTO];


int sock_register(const struct net_proto_family *ops)
{
	if (ops->family >= NPROTO) {
		printk("protocol %d >=  NPROTO (%d)\n", ops->family, NPROTO);
		return -ENOBUFS;
	}

	net_families[ops->family] = ops;
	pr_info("NET: Registered protocol family %d\n", ops->family);
	return 0;
}


struct socket *sock_alloc(void)
{
	struct socket *sock = (struct socket *)kzalloc(sizeof(struct socket), 0);

	/*
	 * Linux normally allocates the socket_wq when calling
	 * sock_alloc_inode() while we do it here hoping for the best.
	 */
	sock->wq = (struct socket_wq*)kzalloc(sizeof(*sock->wq), 0);
	if (!sock->wq) {
		kfree(sock);
		return NULL;
	}

	return sock;
}


int sock_create_lite(int family, int type, int protocol, struct socket **res)

{
	struct socket *sock = sock_alloc();

	if (!sock)
		return -ENOMEM;

	sock->type = type;
	*res = sock;
	return 0;
}


int sock_create_kern(struct net *net, int family, int type, int proto,
                     struct socket **res)
{
	struct socket *sock;
	const struct net_proto_family *pf;
	int err;

	if (family < 0 || family > NPROTO)
		return -EAFNOSUPPORT;

	if (type < 0 || type > SOCK_MAX)
		return -EINVAL;

	pf = net_families[family];

	if (!pf) {
		printk("No protocol found for family %d\n", family);
		return -ENOPROTOOPT;
	}

	sock = sock_alloc();
	if (!sock) {
		printk("Could not allocate socket\n");
		return -ENFILE;
	}

	sock->type = type;

	err = pf->create(&init_net, sock, proto, 1);
	if (err) {
		kfree(sock);
		return err;
	}

	*res = sock;

	return 0;
}


static void sock_init(void)
{
	skb_init();
}


core_initcall(sock_init);


/*************************
 ** Lxip initialization **
 *************************/

/*
 * Header declarations and tuning
 */
struct net init_net;

unsigned long *sysctl_local_reserved_ports;


/**
 * nr_free_buffer_pages - count number of pages beyond high watermark
 *
 * nr_free_buffer_pages() counts the number of pages which are beyond the
 * high
 * watermark within ZONE_DMA and ZONE_NORMAL.
 */
unsigned long nr_free_buffer_pages(void)
{
	return 1000;
}


/*
 * Declare stuff used
 */
int __ip_auto_config_setup(char *addrs);
void core_sock_init(void);
void core_netlink_proto_init(void);
void subsys_net_dev_init(void);
void fs_inet_init(void);
void module_driver_init(void);
void module_cubictcp_register(void);
void late_ip_auto_config(void);
void late_tcp_congestion_default(void);


int __set_thash_entries(char *str);
int __set_uhash_entries(char *str);

static void lxip_kernel_params(void)
{
	__set_thash_entries("2048");
	__set_uhash_entries("2048");
}

/**
 * Initialize sub-systems
 */
void lxip_init()
{
	/* init data */
	INIT_LIST_HEAD(&init_net.dev_base_head);

	core_sock_init();
	core_netlink_proto_init();

	/* sub-systems */
	subsys_net_dev_init();

	lxip_kernel_params();
	fs_inet_init();

	/* enable local accepts */
	IPV4_DEVCONF_ALL(&init_net, ACCEPT_LOCAL) = 0x1;

	/* congestion control */
	module_cubictcp_register();

	/* driver */
	module_driver_init();

	/* late  */
	late_tcp_congestion_default();
}


/*
 * Network configuration
 */
static void lxip_configure(char const *address_config)
{
	__ip_auto_config_setup((char *)address_config);
	late_ip_auto_config();
}


static bool dhcp_configured = false;
static bool dhcp_pending    = false;

void lxip_configure_mtu(unsigned mtu)
{
	/* zero mtu means reset to default */
	unsigned new_mtu = mtu ? mtu : ETH_DATA_LEN;

	struct net        *net;
	struct net_device *dev;

	for_each_net(net) {
		for_each_netdev(net, dev) {
			dev_set_mtu(dev, new_mtu);
		}
	}
}


void lxip_configure_static(char const *addr, char const *netmask,
                           char const *gateway, char const *nameserver)
{
	char address_config[128];

	dhcp_configured = false;

	snprintf(address_config, sizeof(address_config),
	         "%s::%s:%s:::off:%s",
	         addr, gateway, netmask, nameserver);
	lxip_configure(address_config);
}


void lxip_configure_dhcp()
{
	dhcp_configured = true;
	dhcp_pending    = true;

	lxip_configure("dhcp");
	dhcp_pending = false;
}


bool lxip_do_dhcp()
{
	return dhcp_configured && !dhcp_pending;
}


/******************
 ** Lxip private **
 ******************/

void set_sock_wait(struct socket *sock, unsigned long ptr)
{
	sock->sk->sk_wq = (struct socket_wq *)ptr;
}


int socket_check_state(struct socket *socket)
{
	if (socket->sk->sk_state == TCP_CLOSE_WAIT)
		return -EINTR;

	return 0;
}


void log_sock(struct socket *socket)
{
	printk("\nNEW socket %p sk %p fsk %x &sk %p &fsk %p\n\n",
	       socket, socket->sk, socket->flags, &socket->sk, &socket->flags);
}

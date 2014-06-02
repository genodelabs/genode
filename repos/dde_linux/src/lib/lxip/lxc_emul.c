/**
 * \brief  Linux emulation code
 * \author Sebastian Sumpf
 * \date   2013-08-30
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <linux/netdevice.h>
#include <net/ip_fib.h>
#include <uapi/linux/rtnetlink.h>
#include <dde_kit/memory.h>
#include <net/sock.h>
#include <net/route.h>
#include <net/tcp.h>


/********************
 ** linux/slab.h   **
 ********************/

struct kmem_cache
{
	const char *name;  /* cache name */
	unsigned    size;  /* object size */
};


struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
                                     unsigned long falgs, void (*ctor)(void *))
{
	dde_kit_log(DEBUG_SLAB, "\"%s\" obj_size=%zd", name, size);

	struct kmem_cache *cache;

	if (!name) {
		pr_info("kmem_cache name required\n");
		return 0;
	}

	cache = (struct kmem_cache *)kmalloc(sizeof(*cache), 0);
	if (!cache) {
		pr_crit("No memory for slab cache\n");
		return 0;
	}

	cache->name = name;
	cache->size = size;

	return cache;
}


void *kmem_cache_alloc_node(struct kmem_cache *cache, gfp_t flags, int node)
{
	dde_kit_log(DEBUG_SLAB, "\"%s\" alloc obj_size=%u",  cache->name,cache->size);
	return kmalloc(cache->size, 0);
}


void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
	dde_kit_log(DEBUG_SLAB, "\"%s\" alloc obj_size=%u",  cache->name,cache->size);
	return kmalloc(cache->size, 0);
}


void kmem_cache_free(struct kmem_cache *cache, void *objp)
{
	dde_kit_log(DEBUG_SLAB, "\"%s\" (%p)", cache->name, objp);
	kfree(objp);
}


void *alloc_large_system_hash(const char *tablename,
	                            unsigned long bucketsize,
	                            unsigned long numentries,
	                            int scale,
	                            int flags,
	                            unsigned int *_hash_shift,
	                            unsigned int *_hash_mask,
	                            unsigned long low_limit,
	                            unsigned long high_limit)
{
	unsigned long elements = numentries ? numentries : high_limit;
	unsigned long nlog2 = ilog2(elements);
	nlog2 <<= (1 << nlog2) < elements ? 1 : 0;

	void *table = dde_kit_simple_malloc(elements * bucketsize);

	if (_hash_mask)
		*_hash_mask = (1 << nlog2) - 1;
	if (_hash_shift)
		*_hash_shift = nlog2;

	return table;
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
	return kmalloc(size, 0);
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


/**********************
 ** linx/rtnetlink.h **
 **********************/

struct netdev_queue *dev_ingress_queue(struct net_device *dev)
{
	return dev->ingress_queue; 
}


/****************
 ** linux/ip.h **
 ****************/

struct iphdr *ip_hdr(const struct sk_buff *skb)
{
	return (struct iphdr *)skb_network_header(skb);
}


/***********************
 ** linux/netdevice.h **
 ***********************/

struct netdev_queue *netdev_pick_tx(struct net_device *dev, struct sk_buff *skb, void *accel_priv)
{
	return netdev_get_tx_queue(dev, 0);
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

long time_after_eq(long a, long b) { return (a - b) >= 0; }
long time_after(long a, long b) { return (b - a) < 0; }


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


/*****************
 ** linux/uio.h **
 *****************/

int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len)
{
	while (len > 0) {
		if (iov->iov_len) {
			int copy = min_t(unsigned int, iov->iov_len, len);
			if (copy_to_user(iov->iov_base, kdata, copy))
				return -EFAULT;
			kdata += copy;
			len -= copy;
			iov->iov_len -= copy;
			iov->iov_base += copy;
		}
		iov++;
	}

	return 0;
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


/**
 * Misc
 */

void set_sock_wait(struct socket *sock, unsigned long ptr)
{

	sock->sk->sk_wq = (struct socket_wq *)ptr;
}



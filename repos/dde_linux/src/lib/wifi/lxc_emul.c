/*
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-03-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* linux includes */
#include <asm-generic/atomic64.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <linux/skbuff.h>

/* local includes */
#include <lxc.h>


/*************************************
 ** private Lx C env implementation **
 *************************************/

bool is_eapol(struct sk_buff *skb)
{
	return ntohs(skb->protocol) == ETH_P_PAE;
}


struct Skb skb_helper(struct sk_buff *skb)
{
	struct Skb helper;

	skb_push(skb, ETH_HLEN);

	helper.packet      = skb->data;
	helper.packet_size = ETH_HLEN;
	helper.frag        = 0;
	helper.frag_size   = 0;

	/**
	 * If received packets are too large (as of now 128 bytes) the actually
	 * payload is put into a fragment. Otherwise the payload is stored directly
	 * in the sk_buff.
	 */
	if (skb_shinfo(skb)->nr_frags) {
		if (skb_shinfo(skb)->nr_frags > 1)
			printk("more than 1 fragment in skb: %p nr_frags: %d", skb,
			       skb_shinfo(skb)->nr_frags);

		skb_frag_t *f    = &skb_shinfo(skb)->frags[0];
		helper.frag      = skb_frag_address(f);
		helper.frag_size = skb_frag_size(f);
	}
	else
		helper.packet_size += skb->len;

	return helper;
}


extern int verbose_alloc;


struct sk_buff *lxc_alloc_skb(size_t len, size_t headroom)
{
	struct sk_buff *skb = alloc_skb(len + headroom, GFP_KERNEL);
	skb_reserve(skb, headroom);
	return skb;
}


unsigned char *lxc_skb_put(struct sk_buff *skb, size_t len)
{
	return skb_put(skb, len);
}


/****************************
 ** asm-generic/atomic64.h **
 ****************************/

/**
 * This is not atomic on 32bit systems but this is not a problem
 * because we will not be preempted.
 */
long long atomic64_add_return(long long i, atomic64_t *p)
{
	p->counter += i;
	return p->counter;
}


/**********************
 ** linux/refcount.h **
 **********************/

void refcount_add(unsigned int i, refcount_t *r) 
{
	atomic_add(i, &r->refs);
}


void refcount_dec(refcount_t *r)
{
	atomic_dec(&r->refs);
}


bool refcount_dec_and_test(refcount_t *r)
{
	return atomic_dec_and_test(&r->refs);
}


void refcount_inc(refcount_t *r) 
{
	atomic_inc(&r->refs);
}


bool refcount_inc_not_zero(refcount_t *r)
{
	return atomic_add_unless(&r->refs, 1, 0);
}


unsigned int refcount_read(const refcount_t *r)
{
	return atomic_read(&r->refs);
}


void refcount_set(refcount_t *r, unsigned int n)
{
	atomic_set(&r->refs, n);
}

bool refcount_sub_and_test(unsigned int i, refcount_t *r)
{
	return atomic_sub_and_test(i, &r->refs);
}


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

unsigned int hweight32(unsigned int w)
{
	w -= (w >> 1) & 0x55555555;
	w =  (w & 0x33333333) + ((w >> 2) & 0x33333333);
	w =  (w + (w >> 4)) & 0x0f0f0f0f;
	return (w * 0x01010101) >> 24;
}


unsigned long find_last_bit(const unsigned long *addr, unsigned long size)
{
	if (size) {
		unsigned long val = BITMAP_LAST_WORD_MASK(size);
		unsigned long idx = (size-1) / BITS_PER_LONG;

		do {
			val &= addr[idx];
			if (val)
				return idx * BITS_PER_LONG + __fls(val);

			val = ~0ul;
		} while (idx--);
	}
	return size;
}


/*****************************
 ** linux/platform_device.h **
 *****************************/

int platform_device_add_resources(struct platform_device *pdev,
                                  const struct resource *res, unsigned int num)
{
	if (!res || !num) {
		pdev->resource      = NULL;
		pdev->num_resources = 0;
	}

	struct resource *r = NULL;

	if (res) {
		r = kmemdup(res, sizeof(struct resource) * num, GFP_KERNEL);
		if (!r)
			return -ENOMEM;
	}

	kfree(pdev->resource);
	pdev->resource = r;
	pdev->num_resources = num;
	return 0;
}


struct bus_type platform_bus_type = {
	.name = "platform"
};


struct platform_device *platform_device_register_simple(const char *name, int id,
                                                        const struct resource *res,
                                                        unsigned int num)
{
	struct platform_device *pdev = kzalloc(sizeof (struct platform_device), GFP_KERNEL);
	if (!pdev)
		return 0;

	pdev->dev.bus = &platform_bus_type;

	size_t len = strlen(name);
	pdev->name = kzalloc(len + 1, GFP_KERNEL);
	if (!pdev->name)
		goto pdev_out;

	memcpy(pdev->name, name, len);
	pdev->name[len] = 0;
	pdev->id = id;

	int err = platform_device_add_resources(pdev, res, num);
	if (err)
		goto pdev_name_out;

	return pdev;

pdev_name_out:
	kfree(pdev->name);
pdev_out:
	kfree(pdev);

	return 0;
}


/***********************
 ** linux/netdevice.h **
 ***********************/

void netdev_run_todo() {
	__rtnl_unlock();
}


void netif_set_gso_max_size(struct net_device *dev, unsigned int size)
{
	dev->gso_max_size = size;
}


/********************
 ** linux/kernel.h **
 ********************/

unsigned long int_sqrt(unsigned long x)
{
	unsigned long b, m, y = 0;

	if (x <= 1) return x;

	m = 1UL << (BITS_PER_LONG - 2);
	while (m != 0) {
		b = y + m;
		y >>= 1;

		if (x >= b) {
			x -= b;
			y += m;
		}
		m >>= 2;
	}

	return y;
}


/*************************
 ** linux/scatterlist.h **
 *************************/

void sg_chain(struct scatterlist *prv, unsigned int prv_nents,
              struct scatterlist *sgl)
{
	prv[prv_nents - 1].offset    = 0;
	prv[prv_nents - 1].length    = 0;
	prv[prv_nents - 1].page_link = (unsigned long)sgl;

	prv[prv_nents - 1].page_flags |=  0x01;
	prv[prv_nents - 1].page_flags &= ~0x02;
}


void sg_init_table(struct scatterlist *sgl, unsigned int nents)
{
	memset(sgl, 0, sizeof(*sgl) * nents);
	sg_mark_end(&sgl[nents -1]);
}


void sg_mark_end(struct scatterlist *sg)
{
	sg->page_flags |=  0x02;
	sg->page_flags &= ~0x01;
}


struct scatterlist *sg_next(struct scatterlist *sg)
{
	if (sg_is_last(sg))
		return NULL;

	sg++;

	if (sg_is_chain(sg))
		sg = sg_chain_ptr(sg);

	return sg;
}


void sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen)
{
	struct page *page   = &sg->page_dummy;
	sg->page_dummy.addr = (void*)buf;
	sg_set_page(sg, page, buflen, 0);
}


void sg_set_page(struct scatterlist *sg, struct page *page, unsigned int len, unsigned int offset)
{
	sg->page_link = (unsigned long) page;
	sg->offset    = offset;
	sg->length    = len;
}


/****************
 ** net/sock.h **
 ****************/

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
	return (struct socket *)kmalloc(sizeof(struct socket), 0);
}


int sock_create_lite(int family, int type, int protocol, struct socket **res)

{
	struct socket *sock = sock_alloc();

	if (!sock) return -ENOMEM;

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

	if (!(sock = (struct socket *)kzalloc(sizeof(struct socket), 0))) {
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


void log_sock(struct socket *socket)
{
	printk("\nNEW socket %p sk %p fsk %lx &sk %p &fsk %p\n\n",
	       socket, socket->sk, socket->flags, &socket->sk, &socket->flags);
}


static void sock_init(void)
{
	skb_init();
}


core_initcall(sock_init);


/*****************
 ** net/codel.h **
 *****************/

codel_time_t codel_get_time(void)
{
	u64 ns = ktime_get();
	return ns >> CODEL_SHIFT;
}


/**********************
 ** net/codel_impl.h **
 **********************/

void codel_params_init(struct codel_params *params)
{
	params->interval = MS2TIME(100);
	params->target = MS2TIME(5);
	params->ce_threshold = CODEL_DISABLED_THRESHOLD;
	params->ecn = false;
}

void codel_vars_init(struct codel_vars *vars)
{
	memset(vars, 0, sizeof(*vars));
}

void codel_stats_init(struct codel_stats *stats)
{
	 // stats->maxpacket = 0;
}


/*************************
 ** linux/timekeeping.h **
 *************************/

u64 ktime_get_boot_ns(void)
{
	return (u64)ktime_get();
}


/********************
 ** linux/device.h **
 ********************/

struct device *device_create_with_groups(struct class *class,
                                         struct device *parent, dev_t devt,
                                         void *drvdata,
                                         const struct attribute_group **groups,
                                         const char *fmt, ...)
{
	long ret = -ENODEV;
	if (class == NULL || IS_ERR(class)) { goto err; }

	struct device *dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto err;
	}

	return dev;

err:
	return (void*)ret;
}


struct class *__class_create(struct module *owner,
		const char *name,
		struct lock_class_key *key)
{
	struct class *cls = kzalloc(sizeof(*cls), GFP_KERNEL);
	if (!cls) { return (void*)-ENOMEM; }

	cls->name = name;
	return cls;
}

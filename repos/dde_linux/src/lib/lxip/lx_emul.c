/*
 * \brief  Implementation of driver specific Linux functions
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/net.h>
#include <linux/skbuff.h>

unsigned long __FIXADDR_TOP = 0xfffff000;

#include <linux/mm.h>

int mmap_rnd_bits;

#include <linux/percpu.h>

DEFINE_PER_CPU(unsigned long, cpu_scale);


#include <asm/pgtable.h>

#ifndef __arm__
pgd_t reserved_pg_dir[PTRS_PER_PGD];
#endif

bool arm64_use_ng_mappings = false;

pteval_t __default_kernel_pte_mask __read_mostly = ~0;


/* shadowed */
#include <linux/utsname.h>

struct new_utsname init_uts_ns;


#include <linux/random.h>

u8 get_random_u8(void)
{
	return get_random_u32() & 0xff;
}


u16 get_random_u16(void)
{
	return get_random_u32() & 0xffff;
}


#ifdef __arm__
#include <asm/uaccess.h>

unsigned long arm_copy_from_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}

unsigned long arm_copy_to_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif


#include <linux/uaccess.h>

#ifndef INLINE_COPY_TO_USER
unsigned long _copy_from_user(void * to,const void __user * from,unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


unsigned long raw_copy_from_user(void *to, const void * from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#else
unsigned long __must_check __arch_copy_from_user(void *to, const void __user *from, unsigned long n);
unsigned long __must_check __arch_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif


#ifndef INLINE_COPY_FROM_USER
unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


unsigned long raw_copy_to_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#else
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif


__wsum csum_partial_copy_from_user(const void __user *src, void *dst, int len)
{
	memcpy(dst, src, len);
	return csum_partial(dst, len, 0);
}


#include <linux/slab.h>

struct kmem_cache * kmem_cache_create_usercopy(const char * name,
                                               unsigned int size,
                                               unsigned int align,
                                               slab_flags_t flags,
                                               unsigned int useroffset,
                                               unsigned int usersize,
                                               void (* ctor)(void *))
{
	return kmem_cache_create(name, size, align, flags, ctor);
}


#include <linux/stringhash.h>

unsigned int full_name_hash(const void *salt, const char *name, unsigned int len)
{
	unsigned hash = ((unsigned long)salt) & ~0u;
	unsigned i;
	for (i = 0; i < len; i++)
		hash += name[i];

	return hash;
}


#include <linux/memblock.h>

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
	void *table;

	nlog2 <<= (1 << nlog2) < elements ? 1 : 0;

	table = lx_emul_mem_alloc_aligned(elements * bucketsize, PAGE_SIZE);

	if (!table) {
		printk("%s:%d error failed to allocate system hash\n", __func__, __LINE__);
		return NULL;
	}

	if (_hash_mask)
		*_hash_mask = (1 << nlog2) - 1;

	if (_hash_shift)
		*_hash_shift = nlog2;

	return table;
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/gfp.h>

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return (unsigned long)kzalloc(PAGE_SIZE, GFP_KERNEL);
}


#include <linux/gfp.h>

void * page_frag_alloc_align(struct page_frag_cache * nc, unsigned int fragsz,
                             gfp_t gfp_mask, unsigned int align_mask)
{
	if (align_mask != ~0U) {
		printk("page_frag_alloc_align: unsupported align_mask=%x\n", align_mask);
		lx_emul_trace_and_stop(__func__);
	}

	if (fragsz > PAGE_SIZE) {
		printk("page_frag_alloc_align: unsupported fragsz=%u\n", fragsz);
		lx_emul_trace_and_stop(__func__);
	}

	return alloc_pages_exact(PAGE_SIZE, GFP_KERNEL);
}


void page_frag_free(void * addr)
{
	free_pages_exact(addr, PAGE_SIZE);
}


/* mm/page_alloc. */

/**
 * nr_free_buffer_pages - count number of pages beyond high watermark
 *
 * nr_free_buffer_pages() counts the number of pages which are beyond the high
 * watermark within ZONE_DMA and ZONE_NORMAL.
 *
 * used in 'tcp_mem_init'
 *
 * limit = max(nr_free_buffer_pages()/16, 128)
 * -> set to max
 */
unsigned long nr_free_buffer_pages(void)
{
	return 2048;
}


/****************************
 ** Linux socket functions **
 ****************************/



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

	init_waitqueue_head(&sock->wq.wait);
	sock->wq.fasync_list = NULL;
	sock->wq.flags = 0;

	sock->state = SS_UNCONNECTED;
	sock->flags = 0;
	sock->ops = NULL;
	sock->sk = NULL;
	sock->file = NULL;


	return sock;
}


void sock_release(struct socket *sock)
{
	kfree(sock);
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

	err = pf->create(net, sock, proto, 1);
	if (err) {
		kfree(sock);
		return err;
	}

	*res = sock;

	return 0;
}


static int sock_init(void)
{
	skb_init();
	return 0;
}


core_initcall(sock_init);

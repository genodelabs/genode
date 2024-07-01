/*
 * \brief  Implementation of driver specific Linux functions
 * \author Sebastian Sumpf
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <linux/net.h>

pteval_t __default_kernel_pte_mask __read_mostly = ~0;


struct usb_driver usbfs_driver = {
	.name = "usbfs"
};

const struct attribute_group *usb_device_groups[] = { };


#include <net/netfilter/nf_conntrack.h>

struct net init_net;


#include <net/net_namespace.h>

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
	return lx_emul_mem_alloc_aligned(fragsz, ARCH_KMALLOC_MINALIGN);
}


void page_frag_free(void * addr)
{
	lx_emul_mem_free(addr);
}

#include <linux/uaccess.h>

#ifndef INLINE_COPY_TO_USER
unsigned long _copy_from_user(void * to,const void __user * from,unsigned long n)
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
#else
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif


unsigned long arm_copy_from_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}



int netdev_register_kobject(struct net_device * ndev)
{
	return 0;
}


/*
 * \brief  Balloon driver to use Genode's dynamic memory balancing
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-09-19
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/highmem.h>
#include <linux/sizes.h>

#include <asm/tlb.h>

#include <l4/log/log.h>
#include <genode/balloon.h>

#define GFP_BALLOON \
	(__GFP_IO | __GFP_FS | __GFP_HARDWALL | __GFP_HIGHMEM | __GFP_NOWARN | __GFP_NORETRY | __GFP_NOMEMALLOC)

enum { CHUNK_CACHE_SIZE = 16384 };
static void* chunk_cache[CHUNK_CACHE_SIZE];


static void free_avail_pages(unsigned long data)
{
	void *pages;
	unsigned i = 0;

	LOG_printf("free_avail_pages\n");
	for (; i < CHUNK_CACHE_SIZE; i++) {
		pages = alloc_pages_exact(SZ_1M, GFP_BALLOON);
		if (!pages)
			break;
		chunk_cache[i] = pages;
	}

	BUG_ON(i == CHUNK_CACHE_SIZE);

	/* Ensure that ballooned highmem pages don't have kmaps. */
	kmap_flush_unused();
	flush_tlb_all();

	for (; i > 0;) {
		genode_balloon_free_chunk((unsigned long)chunk_cache[--i]);
		free_pages_exact(chunk_cache[i], SZ_1M);
	}

	LOG_printf("free_avail_pages done\n");
	genode_balloon_free_done();
}

DECLARE_TASKLET(free_avail, free_avail_pages, 0);

static irqreturn_t event_interrupt(int irq, void *data)
{
	tasklet_schedule(&free_avail);
	return IRQ_HANDLED;
}


static struct platform_device genode_balloon_device = {
	.name   = "balloon-genode",
};


static int __init balloon_init(void)
{
	int      ret = 0;
	unsigned irq;
	l4_cap_idx_t irq_cap;

	/*
	 * touch the memory eager otherwise we run into trouble
	 * when memory is empty and we balloon
	 */
	memset(&chunk_cache, 0, sizeof(chunk_cache));

	/**
	 * Obtain an IRQ for the device.
	 */
	irq_cap = genode_balloon_irq_cap();
	if ((irq = l4x_register_irq(irq_cap)) < 0)
		return -ENOMEM;
	if ((ret = request_irq(irq, event_interrupt, 0,
						   "Genode balloon", &genode_balloon_device))) {
		printk(KERN_WARNING "%s: request_irq failed: %d\n", __func__, ret);
		return ret;
	}

	ret = platform_device_register(&genode_balloon_device);
	return ret;
}

subsys_initcall(balloon_init);

MODULE_LICENSE("GPL");

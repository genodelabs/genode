/**
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-03-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <asm-generic/atomic64.h>
#include <linux/netdevice.h>
#include <net/sock.h>


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


struct platform_device *platform_device_register_simple(const char *name, int id,
                                                        const struct resource *res,
                                                        unsigned int num)
{
	struct platform_device *pdev = kzalloc(sizeof (struct platform_device), GFP_KERNEL);
	if (!pdev)
		return 0;

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

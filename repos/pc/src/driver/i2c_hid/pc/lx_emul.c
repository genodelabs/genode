/*
 * \brief  Linux emulation environment specific to this driver
 * \author Christian Helmuth
 * \date   2022-05-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/cdev.h>

int cdev_device_add(struct cdev * cdev,struct device * dev)
{
	lx_emul_trace(__func__);
	return device_add(dev);
}


#include <linux/cdev.h>

void cdev_device_del(struct cdev * cdev,struct device * dev)
{
	lx_emul_trace(__func__);
	device_del(dev);
}


#include <linux/cdev.h>

void cdev_init(struct cdev * cdev, const struct file_operations * fops) { }


#include <linux/task_work.h>

int task_work_add(struct task_struct * task,struct callback_head * work,enum task_work_notify_mode notify)
{
	printk("%s: task: %px work: %px notify: %u\n", __func__, task, work, notify);
	return -1;
}


#include <linux/clkdev.h>

#define MAX_DEV_ID	20
#define MAX_CON_ID	16

struct clk_lookup_alloc {
	struct clk_lookup cl;
	char	dev_id[MAX_DEV_ID];
	char	con_id[MAX_CON_ID];
};

struct clk_lookup *clkdev_create(struct clk *clk, const char *con_id, const char *dev_fmt, ...)
{
	struct clk_lookup_alloc *cla;

	cla = kzalloc(sizeof(*cla), GFP_KERNEL);
	if (!cla)
		return NULL;

//	cla->cl.clk_hw = hw;
	if (con_id) {
		strscpy(cla->con_id, con_id, sizeof(cla->con_id));
		cla->cl.con_id = cla->con_id;
	}

	if (dev_fmt) {
		va_list ap;
		va_start(ap, dev_fmt);
		vscnprintf(cla->dev_id, sizeof(cla->dev_id), dev_fmt, ap);
		va_end(ap);
		cla->cl.dev_id = cla->dev_id;
	}

	return &cla->cl;
}


#include <linux/pci.h>

int pci_irq_vector(struct pci_dev * dev,unsigned int nr)
{
	return dev->irq;
}


#include <linux/sched/rt.h>

void rt_mutex_pre_schedule(void)
{
	/* sched_submit_work(current); */
}


void rt_mutex_post_schedule(void)
{
	/* sched_update_worker(current); */
}


void rt_mutex_schedule(void)
{
	do {
		preempt_disable();
		schedule();
		sched_preempt_enable_no_resched();
	} while (need_resched());
}

/*
 * \brief  Linux Kernel initialization
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/init.h>
#include <lx_emul/page_virt.h>
#include <lx_emul/time.h>
#include <lx_user/init.h>

#include <asm/irq_regs.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/init_task.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/tick.h>
#include <linux/of.h>
#include <linux/of_clk.h>
#include <linux/of_fdt.h>
#include <linux/version.h>
#include <net/net_namespace.h>

#include <../mm/slab.h>

/* definitions in drivers/base/base.h */
extern int devices_init(void);
extern int buses_init(void);
extern int classes_init(void);
extern int platform_bus_init(void);
extern int auxiliary_bus_init(void);

/* definition from kernel/main.c implemented architecture specific */
extern void time_init(void);

enum system_states system_state;

static __initdata DECLARE_COMPLETION(kthreadd_done);

static int kernel_init(void * args)
{
	struct task_struct *tsk = current;
	set_task_comm(tsk, "init");

	/* setup page struct for zero page in BSS */
	lx_emul_add_page_range(empty_zero_page, PAGE_SIZE);

	wait_for_completion(&kthreadd_done);

	workqueue_init();

	/* the following calls are from driver_init() of drivers/base/init.c */
	devices_init();
	buses_init();
	classes_init();
	of_core_init();
	platform_bus_init();

	auxiliary_bus_init();

	lx_emul_initcalls();

	system_state = SYSTEM_RUNNING;

	lx_user_init();
	lx_emul_task_schedule(true);
	return 0;
}


static int kernel_idle(void * args)
{
	struct task_struct *tsk = current;
	set_task_comm(tsk, "idle");

	/* set this current task to be the idle task */
	lx_emul_task_set_idle();

	/*
	 * Idle task always gets run in the end of each schedule
	 * and again at the beginning of each schedule
	 */
	for (;;) {
		lx_emul_task_schedule(true);

		tick_nohz_idle_enter();
		tick_nohz_idle_stop_tick();

		lx_emul_task_schedule(true);

		tick_nohz_idle_restart_tick();
		tick_nohz_idle_exit();
	}

	return 0;
}


static void timer_loop(void)
{
	/* set timer interrupt task to highest priority */
	lx_emul_task_priority(current, 0);

	for (;;) {
		lx_emul_task_schedule(true);
		lx_emul_time_handle();
	}
}


int lx_emul_init_task_function(void * dtb)
{
	int pid;

	/* Set dummy task registers used in IRQ and time handling */
	static struct pt_regs regs;
	set_irq_regs(&regs);

	/**
	 * Here we do the minimum normally done start_kernel() of init/main.c
	 */

	jump_label_init();
	kmem_cache_init();
	wait_bit_init();
	radix_tree_init();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,0)
	maple_tree_init();
#endif

	/*
	 * unflatten_device tree requires memblock, so kmem_cache_init has to be
	 * called before lx_emul_setup_arch on ARM platforms
	 */
	lx_emul_setup_arch(dtb);

	workqueue_init_early();

	skb_init();

	early_irq_init();
	irqchip_init();

	tick_init();
	init_timers();
	hrtimers_init();
	softirq_init();
	timekeeping_init();
	time_init();

	sched_clock_init();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
	net_ns_init();
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
	kernel_thread(kernel_init, NULL, "init", CLONE_FS);
	kernel_thread(kernel_idle, NULL, "idle", CLONE_FS);
	pid = kernel_thread(kthreadd, NULL, "kthreadd", CLONE_FS | CLONE_FILES);
#else
	kernel_thread(kernel_init, NULL, CLONE_FS);
	kernel_thread(kernel_idle, NULL, CLONE_FS);
	pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
#endif

	kthreadd_task = find_task_by_pid_ns(pid, NULL);;

	system_state = SYSTEM_SCHEDULING;

	complete(&kthreadd_done);

	lx_emul_task_schedule(false);

	timer_loop();

	return 0;
}


static struct cred _init_task_cred;


struct task_struct init_task = {
	.__state         = 0,
	.usage           = REFCOUNT_INIT(2),
	.flags           = PF_KTHREAD,
	.prio            = MAX_PRIO - 20,
	.static_prio     = MAX_PRIO - 20,
	.normal_prio     = MAX_PRIO - 20,
	.policy          = SCHED_NORMAL,
	.cpus_ptr        = &init_task.cpus_mask,
	.cpus_mask       = CPU_MASK_ALL,
	.nr_cpus_allowed = 1,
	.mm              = NULL,
	.active_mm       = NULL,
	.tasks           = LIST_HEAD_INIT(init_task.tasks),
	.real_parent     = &init_task,
	.parent          = &init_task,
	.children        = LIST_HEAD_INIT(init_task.children),
	.sibling         = LIST_HEAD_INIT(init_task.sibling),
	.group_leader    = &init_task,
	.comm            = INIT_TASK_COMM,
	.thread          = INIT_THREAD,
	.pending         = {
		.list   = LIST_HEAD_INIT(init_task.pending.list),
		.signal = {{0}}
	},
	.blocked         = {{0}},
	.cred            = &_init_task_cred,
};
void * lx_emul_init_task_struct = &init_task;

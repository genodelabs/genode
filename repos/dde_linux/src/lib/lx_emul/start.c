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
#include <linux/tick.h>
#include <linux/of.h>
#include <linux/of_clk.h>
#include <linux/of_fdt.h>

/* definitions in drivers/base/base.h */
extern int devices_init(void);
extern int buses_init(void);
extern int classes_init(void);
extern int platform_bus_init(void);

/* definition from kernel/main.c implemented architecture specific */
extern void time_init(void);

enum system_states system_state;

static __initdata DECLARE_COMPLETION(kthreadd_done);

static int kernel_init(void * args)
{
	struct task_struct *tsk = current;
	set_task_comm(tsk, "init");

	wait_for_completion(&kthreadd_done);

	workqueue_init();

	/* the following calls are from driver_init() of drivers/base/init.c */
	devices_init();
	buses_init();
	classes_init();
	of_core_init();
	platform_bus_init();

	lx_emul_initcalls();

	system_state = SYSTEM_RUNNING;

	lx_user_init();
	lx_emul_task_schedule(true);
	return 0;
}


static void timer_loop(void)
{
	for (;;) {
		tick_nohz_idle_enter();

		if (!lx_emul_task_another_runnable())
			tick_nohz_idle_stop_tick();

		lx_emul_task_schedule(true);
		lx_emul_time_handle();
		tick_nohz_idle_exit();
	}
}


int lx_emul_init_task_function(void * dtb)
{
	int pid;

	/* Set dummy task registers used in IRQ and time handling */
	static struct pt_regs regs;
	set_irq_regs(&regs);

	/* Run emulation library self-tests before starting kernel */
	lx_emul_associate_page_selftest();

	/**
	 * Here we do the minimum normally done start_kernel() of init/main.c
	 */

	lx_emul_setup_arch(dtb);

	jump_label_init();
	kmem_cache_init();
	radix_tree_init();
	workqueue_init_early();

	early_irq_init();
	irqchip_init();

	tick_init();
	init_timers();
	hrtimers_init();
	softirq_init();
	timekeeping_init();

	time_init();

	sched_clock_init();

	kernel_thread(kernel_init, NULL, CLONE_FS);
	pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
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

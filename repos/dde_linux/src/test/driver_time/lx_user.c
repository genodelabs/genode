/*
 * \brief  Post kernel activity
 * \author Alexander Boettcher
 * \date   2022-07-01
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/sched/task.h>

#include <linux/delay.h>

#ifdef __x86_64__
/* test wait_for() macro */
#define CONFIG_DRM_I915_TIMESLICE_DURATION 1
#define CONFIG_DRM_I915_PREEMPT_TIMEOUT    640
#define CONFIG_DRM_I915_HEARTBEAT_INTERVAL 2500

#include "i915_drv.h"
#endif

#include <lx_emul/time.h>


extern uint64_t tsc_freq_khz;


static int timing_tests(void *);


void lx_user_init(void)
{
	kernel_thread(timing_tests, NULL, "lx_user", CLONE_FS | CLONE_FILES);
}


struct measure {
	uint64_t start;
	uint64_t end;
	uint64_t diff;
};

#define test_timing(fn_test, fn_evaluation) \
{ \
	struct measure m_lxemul, m_jiffies, m_rdtsc; \
	uint64_t jiffies_in_us; \
\
	m_lxemul.start  = lx_emul_time_counter(); \
	m_jiffies.start = jiffies_64; \
	m_rdtsc.start   = get_cycles(); \
\
	{ \
		fn_test \
	} \
\
	m_rdtsc.end   = get_cycles(); \
	m_jiffies.end = jiffies_64; \
	m_lxemul.end  = lx_emul_time_counter(); \
\
	m_rdtsc.diff   = m_rdtsc.end   - m_rdtsc.start; \
	m_lxemul.diff  = m_lxemul.end  - m_lxemul.start; \
	m_jiffies.diff = m_jiffies.end - m_jiffies.start; \
\
	jiffies_in_us  = (m_jiffies.diff) * (1000ull * 1000 / CONFIG_HZ); \
\
	{ \
		fn_evaluation \
	} \
\
}

#define test_timing_no_ret(text, fn_test) \
{ \
	test_timing( \
		fn_test \
	, \
		if (rdtsc_freq_mhz) \
			printk(text \
			       " %7llu:%10llu:%10llu:%10llu:%8lld\n", \
			       m_jiffies.diff, jiffies_in_us, m_lxemul.diff, \
			       m_rdtsc.diff / rdtsc_freq_mhz, \
			       jiffies_in_us - m_lxemul.diff); \
		else \
			printk(text \
			       " %7llu:%10llu:%10llu:%8lld\n", \
			       m_jiffies.diff, jiffies_in_us, m_lxemul.diff, \
			       jiffies_in_us - m_lxemul.diff); \
	); \
	/* trigger to update jiffies to avoid printk time part of next test */ \
	msleep(1); \
}


#define test_timing_with_ret(text, fn_test) \
{ \
	test_timing( \
		fn_test \
	, \
		if (rdtsc_freq_mhz) \
			printk(text \
			       " %7llu:%10llu:%10llu:%10llu:%8lld " \
			       "ret=%d%s\n", \
			       m_jiffies.diff, jiffies_in_us, m_lxemul.diff, \
			       m_rdtsc.diff / rdtsc_freq_mhz, \
			       jiffies_in_us - m_lxemul.diff, \
			       ret, ret == -ETIMEDOUT ? " (ETIMEDOUT)" : ""); \
		else \
			printk(text \
			       " %7llu:%10llu:%10llu:%8lld " \
			       "ret=%d%s\n", \
			       m_jiffies.diff, jiffies_in_us, m_lxemul.diff, \
			       jiffies_in_us - m_lxemul.diff, \
			       ret, ret == -ETIMEDOUT ? " (ETIMEDOUT)" : ""); \
	); \
	/* trigger to update jiffies to avoid printk time part of next test */ \
	msleep(1); \
}


static int timing_tests(void * data)
{
	DEFINE_WAIT(wait);
	wait_queue_head_t wq;
	uint64_t const rdtsc_freq_mhz = tsc_freq_khz / 1000;

	init_waitqueue_head(&wq);

	while (true) {
		if (rdtsc_freq_mhz)
			printk("test(parameters)        -> "
			       "jiffies:   jiff_us:lx_time_us:  rdtsc_us:diff_jiff_lx_time "
			       "tsc=%lluMhz\n", rdtsc_freq_mhz);
		else
			printk("test(parameters)        -> "
			       "jiffies:   jiff_us:lx_time_us:diff_jiff_lx_time\n");

		test_timing_no_ret  ("udelay(40)              ->",
			udelay(40);
		);

		test_timing_no_ret  ("ndelay(4000)            ->",
			ndelay(4000);
		);

		test_timing_no_ret  ("msleep(5000)            ->",
			msleep(5000);
		);

#ifdef __x86_64__
{
		int ret = 0;

		test_timing_with_ret("wait_for(cond,10ms) A   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 10);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,5ms)  B   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 5);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,2ms)  C   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 2);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,10ms) D   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 10);
			remove_wait_queue(&wq, &wait);
		);

		/* do some work, so that jiffies becomes a bit outdated */
		{
			unsigned long long i = 0;
			printk("cause some long running load in task ...\n");
			for (i = 0; i < (1ull << 24); i++) {
				asm volatile("pause":::"memory");
			}
		}

		/* display driver test case -> waking up too early before irq triggered */
		test_timing_with_ret("wait_for(cond,10ms) E   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 10);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,5ms)  F   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 5);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,2ms)  G   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 2);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,10ms) H   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 10);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,5000ms)   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 5000);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,4000ms)   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 4000);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,3000ms)   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 3000);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,2000ms)   ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 2000);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,500ms)    ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 500);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,200ms)    ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 200);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,100ms)    ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 100);
			remove_wait_queue(&wq, &wait);
		);

		test_timing_with_ret("wait_for(cond,50ms)     ->",
			add_wait_queue(&wq, &wait);
			ret = wait_for((0), 50);
			remove_wait_queue(&wq, &wait);
		);
}
#else
		printk("skip x86_64 wait_for() tests ...\n");
#endif

		/* audio driver test case -> sleeping too short or long is bad */
		test_timing_no_ret  ("usleep_range(20,21)     ->",
			usleep_range(20, 21);
		);

		test_timing_no_ret  ("usleep_range(40,41)     ->",
			usleep_range(40, 41);
		);

		test_timing_no_ret  ("usleep_range(400,410)   ->",
			usleep_range(400, 410);
		);

		/* wifi driver use case -> iwl_trans_pcie_sw_reset */
		test_timing_no_ret  ("usleep_range(5000,6000) ->",
			usleep_range(5000, 6000);
		);
	}

	return 0;
}

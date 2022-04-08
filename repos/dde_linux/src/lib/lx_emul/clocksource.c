/*
 * \brief  Linux DDE timer
 * \author Stefan Kalkowski
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/timecounter.h>
#include <linux/sched_clock.h>
#include <linux/smp.h>
#include <linux/of_clk.h>
#include <lx_emul/debug.h>
#include <lx_emul/time.h>
#include <lx_emul/init.h>

static u32 dde_timer_rate = 1000000; /* we use microseconds as rate */


static int dde_set_next_event(unsigned long evt,
                              struct clock_event_device *clk)
{
	lx_emul_time_event(evt);
	return 0;
}


static int dde_set_state_shutdown(struct clock_event_device *clk)
{
	lx_emul_time_stop();
	return 0;
}


static u64 dde_timer_read_counter(void)
{
	return lx_emul_time_counter();
}


static u64 dde_clocksource_read_counter(struct clocksource * cs)
{
	return lx_emul_time_counter();
}


static u64 dde_cyclecounter_read_counter(const struct cyclecounter * cc)
{
	return lx_emul_time_counter();
}


static struct clock_event_device * dde_clock_event_device;


void lx_emul_time_init()
{
	static struct clocksource clocksource = {
		.name   = "dde_counter",
		.rating = 400,
		.read   = dde_clocksource_read_counter,
		.mask   = CLOCKSOURCE_MASK(56),
		.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
	};

	static struct cyclecounter cyclecounter = {
		.read = dde_cyclecounter_read_counter,
		.mask = CLOCKSOURCE_MASK(56),
	};

	static struct timecounter timecounter;

	static struct clock_event_device clock_event_device = {
		.name                      = "dde_timer",
		.features                  = CLOCK_EVT_FEAT_ONESHOT | CLOCK_SOURCE_VALID_FOR_HRES,
		.rating                    = 400,
		.set_state_shutdown        = dde_set_state_shutdown,
		.set_state_oneshot_stopped = dde_set_state_shutdown,
		.set_next_event            = dde_set_next_event,
	};

	u64 start_count            = dde_timer_read_counter();
	clock_event_device.cpumask = cpumask_of(smp_processor_id()),
	dde_clock_event_device     = &clock_event_device;

	clocksource_register_hz(&clocksource, dde_timer_rate);
	cyclecounter.mult  = clocksource.mult;
	cyclecounter.shift = clocksource.shift;
	timecounter_init(&timecounter, &cyclecounter, start_count);
	clockevents_config_and_register(&clock_event_device, dde_timer_rate, 0xf, 0x7fffffff);
	sched_clock_register(dde_timer_read_counter, 64, dde_timer_rate);

	/* execute setup calls of clock providers in __clk_of_table */
	of_clk_init(NULL);

}


void lx_emul_time_handle(void)
{
	dde_clock_event_device->event_handler(dde_clock_event_device);
}


enum { LX_EMUL_MAX_OF_CLOCK_PROVIDERS = 256 };


struct of_device_id __clk_of_table[LX_EMUL_MAX_OF_CLOCK_PROVIDERS] = { };


void lx_emul_register_of_clk_initcall(char const *compat, void *fn)
{
	static unsigned count;

	if (count == LX_EMUL_MAX_OF_CLOCK_PROVIDERS) {
		printk("lx_emul_register_of_clk_initcall: __clk_of_table exhausted\n");
		return;
	}

	strncpy(__clk_of_table[count].compatible, compat,
	        sizeof(__clk_of_table[count].compatible));

	__clk_of_table[count].data = fn;

	count++;
}


/*
 * The functions lx_emul_force_jiffies_update, lx_clockevents_program_event
 * and lx_clockevents_program_event implemented here are stripped down
 * versions of the Linux internal time handling code, when clock is used in
 * periodic mode.
 *
 * Normally time proceeds due to
 * -> lx_emul/shadow/kernel/sched/core.c calls lx_emul_time_handle()
 *   -> repos/dde_linux/src/lib/lx_emul/clocksource.c:lx_emul_time_handle() calls
 *     -> dde_clock_event_device->event_handle() == tick_handle_periodic()
 * -> kernel/time/tick-common.c -> tick_handle_periodic()
 *   -> kernel/time/clockevents.c -> clockevents_program_event()
 *    -> which fast forwards jiffies in a loop until it matches wall clock
 *
 * lx_emul_force_jiffies_update can be used to update jiffies to current,
 * before invoking schedule_timeout(), which expects current jiffies values.
 * Without current jiffies, the programmed timeouts are too short, which leads
 * to timeouts firing too early.
 */


/* kernel/time/timekeeping.h */
extern int  timekeeping_valid_for_hres(void);
extern void do_timer(unsigned long ticks);


/**
 * based on kernel/time/clockevents.c clockevents_program_event()
 */
static int lx_clockevents_program_event(struct clock_event_device *dev,
                                        ktime_t expires)
{
	int64_t delta;

	if (WARN_ON_ONCE(expires < 0))
		return 0;

	dev->next_event = expires;

	if (clockevent_state_shutdown(dev))
		return 0;

	if (dev->features & CLOCK_EVT_FEAT_KTIME)
		return 0;

	delta = ktime_to_ns(ktime_sub(expires, ktime_get()));
	if (delta <= 0) {
		int res = -ETIME;
		return res;
	}

	return 0;
}


/*
 * private kernel/time header needed for internal functions
 * used in lx_emul_force_jiffies_update
 */
#include <../kernel/time/tick-internal.h>

/**
 * based on kernel/time/tick-common.c tick_handle_periodic()
 */
void lx_emul_force_jiffies_update(void)
{
	struct clock_event_device *dev = dde_clock_event_device;

	ktime_t next = dev->next_event;

#if defined(CONFIG_HIGH_RES_TIMERS) || defined(CONFIG_NO_HZ_COMMON)
	/*
	 * The cpu might have transitioned to HIGHRES or NOHZ mode via
	 * update_process_times() -> run_local_timers() ->
	 * hrtimer_run_queues().
	 */
	if (dev->event_handler != tick_handle_periodic)
		return;
#endif

	if (!clockevent_state_oneshot(dev))
		return;

	for (;;) {
		/*
		 * Setup the next period for devices, which do not have
		 * periodic mode:
		 */
		next = ktime_add_ns(next, TICK_NSEC);

		if (!lx_clockevents_program_event(dev, next))
			return;

		/*
		 * Have to be careful here. If we're in oneshot mode,
		 * before we call tick_periodic() in a loop, we need
		 * to be sure we're using a real hardware clocksource.
		 * Otherwise we could get trapped in an infinite
		 * loop, as the tick_periodic() increments jiffies,
		 * which then will increment time, possibly causing
		 * the loop to trigger again and again.
		 */
		if (timekeeping_valid_for_hres()) {
			do_timer(1);  /* tick_periodic(cpu); */
		}
	}
}

/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*******************
 ** linux/timer.h **
 *******************/

struct tvec_base;
extern struct tvec_base boot_tvec_bases;  /* needed by 'dwc_common_linux.c' */

struct timer_list
{
	void (*function)(unsigned long);
	unsigned long data;
	void *timer;
	unsigned long expires;
	struct tvec_base *base;  /* needed by 'dwc_common_linux.c' */
};

void init_timer(struct timer_list *);
void init_timer_deferrable(struct timer_list *);
int mod_timer(struct timer_list *timer, unsigned long expires);
int del_timer(struct timer_list * timer);
void setup_timer(struct timer_list *timer, void (*function)(unsigned long),
                 unsigned long data);

int timer_pending(const struct timer_list * timer);
unsigned long round_jiffies(unsigned long j);
unsigned long round_jiffies_relative(unsigned long j);
unsigned long round_jiffies_up(unsigned long j);

void set_timer_slack(struct timer_list *time, int slack_hz);
static inline void add_timer(struct timer_list *timer) { mod_timer(timer, timer->expires); }

static inline
int del_timer_sync(struct timer_list * timer) { return del_timer(timer); }


/*********************
 ** linux/hrtimer.h **
 *********************/

enum hrtimer_mode { HRTIMER_MODE_ABS = 0 };
enum hrtimer_restart { HRTIMER_NORESTART = 0 };

struct hrtimer
{
	enum hrtimer_restart (*function)(struct hrtimer *);
	struct hrtimer        *data;
	void                  *timer;
};

int hrtimer_start_range_ns(struct hrtimer *, ktime_t,
                          unsigned long, const enum hrtimer_mode);

void hrtimer_init(struct hrtimer *, clockid_t, enum hrtimer_mode);
int hrtimer_cancel(struct hrtimer *);

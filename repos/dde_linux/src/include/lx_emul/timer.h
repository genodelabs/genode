/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Stefan Kalkowski
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
	unsigned long expires;
	void          (*function)(struct timer_list*);
	unsigned int  flags;

	/*
	 * For compatibility with 4.4.3 drivers, the 'data' member is kept and
	 * passed as argument to the callback function. Since the 4.16.3 callback
	 * function interface has a 'timer_list' pointer as argument, the 'data'
	 * member points to the 'timer_list' object itself when set up with the
	 * 'timer_setup()' function.
	 */
	unsigned long data;
};

int  mod_timer(struct timer_list *timer, unsigned long expires);
int  del_timer(struct timer_list * timer);
void timer_setup(struct timer_list *timer,
                 void (*callback)(struct timer_list *), unsigned int flags);
int  timer_pending(const struct timer_list * timer);
unsigned long round_jiffies(unsigned long j);
unsigned long round_jiffies_relative(unsigned long j);
unsigned long round_jiffies_up(unsigned long j);
static inline void add_timer(struct timer_list *timer) {
	mod_timer(timer, timer->expires); }
static inline int  del_timer_sync(struct timer_list * timer) {
	return del_timer(timer); }


/*********************
 ** linux/hrtimer.h **
 *********************/

enum hrtimer_mode {
	HRTIMER_MODE_ABS = 0,
	HRTIMER_MODE_REL = 0x1,
	HRTIMER_MODE_REL_PINNED = 0x03,
};
enum hrtimer_restart {
	HRTIMER_NORESTART,
	HRTIMER_RESTART,
};

struct hrtimer
{
	enum hrtimer_restart (*function)(struct hrtimer *);
	struct hrtimer        *data;
	void                  *timer;
};

int  hrtimer_start_range_ns(struct hrtimer *, ktime_t,
                            unsigned long, const enum hrtimer_mode);

void hrtimer_init(struct hrtimer *, clockid_t, enum hrtimer_mode);
int  hrtimer_cancel(struct hrtimer *);
bool hrtimer_active(const struct hrtimer *);

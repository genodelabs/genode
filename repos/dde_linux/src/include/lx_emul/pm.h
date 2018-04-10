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

/****************
 ** linux/pm.h **
 ****************/

struct device;

typedef struct pm_message { int event; } pm_message_t;

struct dev_pm_info
{
	pm_message_t power_state;
	bool         is_prepared:1;
	bool         is_suspended:1;
	atomic_t     usage_count;
};

struct dev_pm_ops {
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
	int (*freeze)(struct device *dev);
	int (*thaw)(struct device *dev);
	int (*poweroff)(struct device *dev);
	int (*restore)(struct device *dev);
	int (*runtime_suspend)(struct device *dev);
	int (*runtime_resume)(struct device *dev);
	int (*suspend_late)(struct device *dev);
	int (*resume_early)(struct device *dev);
	int (*freeze_late)(struct device *dev);
	int (*thaw_early)(struct device *dev);
	int (*poweroff_late)(struct device *dev);
	int (*restore_early)(struct device *dev);
};

#define PMSG_IS_AUTO(msg) 0

enum { PM_EVENT_AUTO_SUSPEND = 0x402 };

#define PM_EVENT_FREEZE    0x0001
#define PM_EVENT_SUSPEND   0x0002

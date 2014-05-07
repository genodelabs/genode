/*
 * \brief  Genode RTC driver
 * \author Stefan Kalkowski <kalkowski@genode-labs.com>
 * \date   2011-10-25
 *
 * Dummy driver taken from drivers/rtc/rtc-test.c.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/rtc.h>

#include <l4/sys/kdebug.h>

struct genode_rtc {
	struct rtc_device *rtc;
};


static int genode_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	return 0;
}


static int genode_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	return 0;
}


static int genode_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	rtc_time_to_tm(get_seconds(), tm);
	return 0;
}


static int genode_rtc_set_mmss(struct device *dev, unsigned long secs)
{
	dev_info(dev, "%s, secs = %lu\n", __func__, secs);
	return 0;
}


static int genode_rtc_proc(struct device *dev, struct seq_file *seq)
{
	struct platform_device *plat_dev = to_platform_device(dev);

	seq_printf(seq, "genode_rtc\t\t: yes\n");
	seq_printf(seq, "id\t\t: %d\n", plat_dev->id);
	return 0;
}


static int genode_rtc_alarm_irq_enable(struct device *dev, unsigned int enable)
{
	return 0;
}


static const struct rtc_class_ops genode_rtc_ops = {
	.proc             = genode_rtc_proc,
	.read_time        = genode_rtc_read_time,
	.read_alarm       = genode_rtc_read_alarm,
	.set_alarm        = genode_rtc_set_alarm,
	.set_mmss         = genode_rtc_set_mmss,
	.alarm_irq_enable = genode_rtc_alarm_irq_enable,
};


static int genode_rtc_remove(struct platform_device *pdev)
{
	struct genode_rtc *priv = platform_get_drvdata(pdev);
	rtc_device_unregister(priv->rtc);
	platform_set_drvdata(pdev, NULL);
	kfree(priv);
	return 0;
}


static int genode_rtc_probe(struct platform_device *pdev)
{
	int err;
	struct genode_rtc *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	platform_set_drvdata(pdev, priv);

	priv->rtc = rtc_device_register(pdev->name,
	                          &pdev->dev, &genode_rtc_ops, THIS_MODULE);
	if (IS_ERR(priv->rtc)) {
		err = PTR_ERR(priv->rtc);
		return err;
	}
	return 0;
}


static struct platform_driver __refdata genode_rtc_driver = {
	.remove = genode_rtc_remove,
	.probe  = genode_rtc_probe,
	.driver = {
		.name  = "rtc-genode",
		.owner = THIS_MODULE,
	},
};

static struct platform_device genode_rtc_device = {
	.name   = "rtc-genode",
};


static int __init genode_rtc_init(void)
{
	int ret = platform_driver_register(&genode_rtc_driver);
	if (!ret) {
		ret = platform_device_register(&genode_rtc_device);
		if (ret)
			platform_driver_unregister(&genode_rtc_driver);
	}
	return ret;
}
module_init(genode_rtc_init);


static void __exit genode_rtc_exit(void)
{
	platform_device_register(&genode_rtc_device);
	platform_driver_unregister(&genode_rtc_driver);
}
module_exit(genode_rtc_exit);


MODULE_AUTHOR("Stefan Kalkowski <stefan.kalkowski@genode-labs.com>");
MODULE_DESCRIPTION("RTC driver for Genode");
MODULE_LICENSE("GPL");

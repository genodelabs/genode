/*
 * \brief  Post kernel activity
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/sched/task.h>
#include <linux/usb.h>
#include <lx_user/init.h>
#include <genode_c_api/uplink.h>
#include <genode_c_api/mac_address_reporter.h>
#include <usb_net.h>


struct task_struct *lx_user_new_usb_task(int (*func)(void*), void *args,
                                         char const *name)
{
	int pid = kernel_thread(func, args, name, CLONE_FS | CLONE_FILES);
	return find_task_by_pid_ns(pid, NULL);
}


static struct genode_uplink *dev_genode_uplink(struct net_device *dev)
{
	return (struct genode_uplink *)dev->ifalias;
}


struct genode_uplink_rx_context
{
	struct net_device *dev;
};


struct genode_uplink_tx_packet_context
{
	struct sk_buff *skb;
};


static unsigned long uplink_tx_packet_content(struct genode_uplink_tx_packet_context *ctx,
                                              char *dst, unsigned long dst_len)
{
	struct sk_buff * const skb = ctx->skb;

	skb_push(skb, ETH_HLEN);

	if (dst_len < skb->len) {
		printk("uplink_tx_packet_content: packet exceeds uplink packet size\n");
		memset(dst, 0, dst_len);
		return 0;
	}

	skb_copy_from_linear_data(skb, dst, skb->len);

	/* clear unused part of the destination buffer */
	memset(dst + skb->len, 0, dst_len - skb->len);

	return skb->len;
}


static rx_handler_result_t handle_rx(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct net_device *dev = skb->dev;
	struct genode_uplink_tx_packet_context ctx = { .skb = skb };

	/* if uplink still exists */
	if (dev->ifalias) {
		bool progress = genode_uplink_tx_packet(dev_genode_uplink(dev),
		                                        uplink_tx_packet_content,
		                                        &ctx);
		if (!progress)
			printk("handle_rx: uplink saturated, dropping packet\n");
	}

	kfree_skb(skb);
	return RX_HANDLER_CONSUMED;
}


/**
 * Create Genode uplink for given net device
 *
 * The uplink is registered at the dev->ifalias pointer.
 */
static void handle_create_uplink(struct net_device *dev)
{
	struct genode_uplink_args args;

	if (dev_genode_uplink(dev))
		return;

	if (!netif_carrier_ok(dev))
		return;

	printk("create uplink for net device %s\n", &dev->name[0]);

	memset(&args, 0, sizeof(args));

	if (dev->addr_len != sizeof(args.mac_address)) {
		printk("error: net device has unexpected addr_len %u\n", dev->addr_len);
		return;
	}

	{
		unsigned i;
		for (i = 0; i < dev->addr_len; i++)
			args.mac_address[i] = dev->dev_addr[i];
	}

	args.label = &dev->name[0];

	dev->ifalias = (struct dev_ifalias *)genode_uplink_create(&args);
}


static void handle_destroy_uplink(struct net_device *dev)
{
	struct genode_uplink *uplink = dev_genode_uplink(dev);

	if (!uplink)
		return;

	if (netif_carrier_ok(dev))
		return;

	printk("destroy uplink for net device %s\n", &dev->name[0]);
	genode_uplink_destroy(uplink);

	dev->ifalias = NULL;
}


static genode_uplink_rx_result_t uplink_rx_one_packet(struct genode_uplink_rx_context *ctx,
                                                      char const *ptr, unsigned long len)
{
	struct sk_buff *skb = alloc_skb(len, GFP_KERNEL);

	if (!skb) {
		printk("alloc_skb failed\n");
		return GENODE_UPLINK_RX_RETRY;
	}

	skb_copy_to_linear_data(skb, ptr, len);
	skb_put(skb, len);
	skb->dev = ctx->dev;

	if (dev_queue_xmit(skb) < 0) {
		printk("lx_user: failed to xmit packet\n");
		return GENODE_UPLINK_RX_REJECTED;
	}

	return GENODE_UPLINK_RX_ACCEPTED;
}


/*
 * custom MAC address
 */
bool          use_mac_address;
unsigned char mac_address[6];
static bool   mac_address_configured = false;

static void handle_mac_address(struct net_device *dev)
{
	int err;
	struct sockaddr addr;
	struct genode_mac_address dev_addr;

	if (mac_address_configured || !netif_device_present(dev)) return;

	if (use_mac_address) {
		memcpy(&addr.sa_data, mac_address, ETH_ALEN);
		addr.sa_family = dev->type;
		err = dev_set_mac_address(dev, &addr,  NULL);
		if (err < 0)
			printk("Warning: Could not set configured MAC address: %pM (err=%d)\n",
			       mac_address, err);
	}

	memcpy(dev_addr.addr, dev->dev_addr, sizeof(dev_addr));
	genode_mac_address_register(dev->name, dev_addr);

	mac_address_configured =true;
}


static int network_loop(void *arg)
{
	for (;;) {

		struct net_device *dev;

		for_each_netdev(&init_net, dev) {

			handle_mac_address(dev);

			/* enable link sensing, repeated calls are handled by testing IFF_UP */
			dev_open(dev, 0);

			/* install rx handler once */
			if (!netdev_is_rx_handler_busy(dev))
				netdev_rx_handler_register(dev, handle_rx, NULL);

			/* respond to cable plug/unplug */
			handle_create_uplink(dev);
			handle_destroy_uplink(dev);

			/* transmit packets received from the uplink session */
			if (netif_carrier_ok(dev)) {

				struct genode_uplink_rx_context ctx = { .dev = dev };

				while (genode_uplink_rx(dev_genode_uplink(dev),
				                        uplink_rx_one_packet,
				                        &ctx));
			}
		};

		/* block until lx_emul_task_unblock */
		lx_emul_task_schedule(true);
	}
	return 0;
}


static struct task_struct *net_task;

void lx_user_init(void)
{
	pid_t pid;

	lx_emul_usb_client_init();

	pid = kernel_thread(network_loop, NULL, "network_loop",
	                    CLONE_FS | CLONE_FILES);
	net_task = find_task_by_pid_ns(pid, NULL);
}


void lx_user_handle_io(void)
{
	lx_emul_usb_client_ticker();
	if (net_task) lx_emul_task_unblock(net_task);
}


#include <linux/rtnetlink.h>

/*
 * Called whenever the link state changes
 */

static bool force_uplink_destroy = false;

void rtmsg_ifinfo(int type, struct net_device * dev,
                  unsigned int change, gfp_t flags,
                  u32 portid, const struct nlmsghdr *nlh)
{
	/* trigger handle_create_uplink / handle_destroy_uplink */
	if (net_task) lx_emul_task_unblock(net_task);

	if (force_uplink_destroy) {
		struct genode_uplink *uplink = dev_genode_uplink(dev);
		printk("force destroy uplink for net device %s\n", &dev->name[0]);
		genode_uplink_destroy(uplink);
		force_uplink_destroy = false;
	}
}


void lx_emul_usb_client_device_unregister_callback(struct usb_device *)
{
	force_uplink_destroy   = true;
	mac_address_configured = false;
}


/*
 * Handle WDM device class for MBIM-modems
 */

struct usb_class_driver *wdm_driver;
struct file wdm_file;

enum { WDM_MINOR = 8 };

int usb_register_dev(struct usb_interface *intf, struct usb_class_driver *class_driver)
{
	if (strncmp(class_driver->name, "cdc-wdm", 7) == 0) {
		wdm_driver = class_driver;

		intf->usb_dev = &intf->dev;
		intf->minor   = WDM_MINOR;

		lx_wdm_create_root();
		return 0;
	}

	printk("%s:%d error: no device class for driver %s\n", __func__, __LINE__,
	       class_driver->name);

	return -1;
}


void usb_deregister_dev(struct usb_interface * intf,struct usb_class_driver * class_driver)
{
	lx_emul_trace(__func__);
}



int lx_wdm_read(void *args)
{
	ssize_t length;
	struct lx_wdm *wdm_data = (struct lx_wdm *)args;

	lx_emul_task_schedule(true);

	if (!wdm_driver) {
		printk("%s:%d error: no WDM class driver\n", __func__, __LINE__);
		return -1;
	}

	while (wdm_data->active) {
		length = wdm_driver->fops->read(&wdm_file, wdm_data->buffer, 0x1000, NULL);
		if (length > 0) {
			*wdm_data->data_avail = length;
			lx_wdm_signal_data_avail(wdm_data->handle);
		}
		lx_emul_task_schedule(true);
	}

	return 0;
}


int lx_wdm_write(void *args)
{
	ssize_t length;
	struct lx_wdm *wdm_data = (struct lx_wdm *)args;

	lx_emul_task_schedule(true);

	if (!wdm_driver) {
		printk("%s:%d error: no WDM class driver\n", __func__, __LINE__);
		return -1;
	}

	while (wdm_data->active) {
		length = wdm_driver->fops->write(&wdm_file, wdm_data->buffer,
		                                 *wdm_data->data_avail, NULL);
		if (length < 0) {
			printk("WDM write error: %ld", (long)length);
		}

		lx_wdm_schedule_read(wdm_data->handle);
		lx_emul_task_schedule(true);
	}
	return 0;
}


int lx_wdm_device(void *args)
{
	int err = -1;

	/* minor number for inode is 1 (see: ubs_register_dev above) */
	struct inode inode;
	inode.i_rdev = MKDEV(USB_DEVICE_MAJOR, WDM_MINOR);

	if (!wdm_driver) {
		printk("%s:%d error: no WDM class driver\n", __func__, __LINE__);
		return err;
	}

	if ((err = wdm_driver->fops->open(&inode, &wdm_file))) {
		printk("Could not open WDM device: %d", err);
		return err;
	}

	lx_emul_task_schedule(true);
	//XXX: close
	return 0;
}

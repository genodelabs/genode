/*
 * \brief  Wireless LAN driver uplink back end
 * \author Norman Feske
 * \author Josef Soentgen
 * \date   2021-06-02
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include "lx_user.h"

#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <lx_user/init.h>
#include <genode_c_api/uplink.h>


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
	unsigned long total  = 0;
	unsigned long result = 0;
	unsigned long linear = 0;

	/*
	 * We always get the ethernet header from the headroom. In case
	 * the payload is stored in frags we have to copy them as well.
	 */

	/* get ethernet header from head before calling skb_headlen */
	skb_push(skb, ETH_HLEN);
	total = skb->len;

	if (dst_len < total) {
		printk("uplink_tx_packet_content: packet exceeds uplink packet size\n");
		memset(dst, 0, dst_len);
		return 0;
	}

	linear = min_t(int, skb_headlen(skb), total);
	skb_copy_from_linear_data(skb, dst, linear);

	total  -= linear;
	result += linear;

	if (total && skb_shinfo(skb)->nr_frags) {
		unsigned int i;

		for (i = skb_shinfo(skb)->nr_frags - 1; (int)i >= 0; i--) {
			unsigned int size = skb_frag_size(&skb_shinfo(skb)->frags[i]);
			void const * frag = skb_frag_address_safe(&skb_shinfo(skb)->frags[i]);

			memcpy(dst + result, frag, size);
			result += size;
		}
	}

	return result;
}


static rx_handler_result_t handle_rx(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct net_device *dev = skb->dev;
	struct genode_uplink_tx_packet_context ctx = { .skb = skb };

	/*
	 * Pass on EAPOL related frames to send them to the
	 * wpa_supplicant.
	 */
	if (ntohs(skb->protocol) == ETH_P_PAE)
		return RX_HANDLER_PASS;

	{
		bool uplink_available = !!dev_genode_uplink(dev);
		bool progress = uplink_available &&
		                genode_uplink_tx_packet(dev_genode_uplink(dev),
		                                        uplink_tx_packet_content,
		                                        &ctx);
		if (!progress && uplink_available)
			printk("handle_rx: uplink saturated, dropping packet\n");

		if (progress)
			genode_uplink_notify_peers();
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

	genode_uplink_destroy(uplink);

	dev->ifalias = NULL;
}


static genode_uplink_rx_result_t uplink_rx_one_packet(struct genode_uplink_rx_context *ctx,
                                                      char const *ptr, unsigned long len)
{
	struct sk_buff *skb = alloc_skb(len + 128, GFP_KERNEL);
	skb_reserve(skb, 128);

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



struct task_struct *uplink_task_struct_ptr;  /* used by 'Main' for lx_emul_task_unblock */


#include <linux/notifier.h>

struct netdev_event_notification
{
	struct notifier_block      nb;
	struct netdev_net_notifier nn;

	bool registered;
};


/* needed for RFKILL state update */
extern struct task_struct *rfkill_task_struct_ptr;


static int uplink_netdev_event(struct notifier_block *this,
                               unsigned long event, void *ptr)
{
	/*
	 * For now we ignore what kind of event occurred and simply
	 * unblock the uplink and rfkill task.
	 */

	if (uplink_task_struct_ptr)
		lx_emul_task_unblock(uplink_task_struct_ptr);

	if (rfkill_task_struct_ptr)
		lx_emul_task_unblock(rfkill_task_struct_ptr);

	return NOTIFY_DONE;
}


static int new_device_netdev_event(struct notifier_block *this,
                                   unsigned long event, void *ptr)
{
	if (event == NETDEV_REGISTER)
		if (uplink_task_struct_ptr)
			lx_emul_task_unblock(uplink_task_struct_ptr);

	return NOTIFY_DONE;
}


static struct notifier_block new_device_netdev_notifier = {
    .notifier_call = new_device_netdev_event,
};


/* implemented by wlan.cc */
extern void wakeup_wpa(void);


static int user_task_function(void *arg)
{
	struct netdev_event_notification events;
	memset(&events, 0, sizeof (struct netdev_event_notification));

	events.nb.notifier_call = uplink_netdev_event;
	events.registered       = false;

	if (register_netdevice_notifier(&new_device_netdev_notifier)) {
		printk("%s:%d: could not register netdev notifer for "
		       "new devices, abort\n", __func__, __LINE__);
		return -1;
	}

	for (;;) {

		struct net_device *dev;

		for_each_netdev(&init_net, dev) {

			/* there might be more devices, e.g. 'lo', in the netnamespace */
			if (strcmp(&dev->name[0], "wlan0") != 0)
				continue;

			/* enable link sensing, repeated calls are handled by testing IFF_UP */
			if (dev_open(dev, 0) == 0)
				wakeup_wpa();

			/* install rx handler once */
			if (!netdev_is_rx_handler_busy(dev))
				netdev_rx_handler_register(dev, handle_rx, NULL);

			/* register notifier once */
			if (!events.registered) {
				events.registered =
					!register_netdevice_notifier_dev_net(dev,
					                                     &events.nb,
					                                     &events.nn);
			}

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


void uplink_init(void)
{
	pid_t pid;

	pid = kernel_thread(user_task_function, NULL, CLONE_FS | CLONE_FILES);

	uplink_task_struct_ptr = find_task_by_pid_ns(pid, NULL);
}

/*
 * \brief  NIC driver to access Genode's nic service
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2010-09-09
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <linux/etherdevice.h>
#include <linux/errno.h>
#include <linux/inet.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/types.h>

#include <genode/net.h>

static struct net_device *net_dev;


static void genode_net_receive_packet(void* dev_addr, void *addr,
                                      unsigned long size)
{
	struct net_device       *dev   = (struct net_device *) dev_addr;
	struct net_device_stats *stats = (struct net_device_stats*) netdev_priv(dev);

	/* allocate skb */
	struct sk_buff *skb = dev_alloc_skb(size + 4);
	if (!skb) {
		if (printk_ratelimit())
			printk(KERN_NOTICE "genode_net_rx: low on mem - packet dropped!\n");
		stats->rx_dropped++;
		return;
	}

	/* copy packet */
	genode_net_memcpy(skb_put(skb, size), addr, size);

	skb->dev       = dev;
	skb->protocol  = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_NONE;

	netif_rx(skb);

	stats->rx_packets++;
	stats->rx_bytes += size;
}


/********************************
 **  Network driver functions  **
 ********************************/

int genode_net_open(struct net_device *dev)
{
	genode_net_start(dev, genode_net_receive_packet);
	netif_start_queue(dev);
	return 0;
}


int genode_net_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	genode_net_stop();
	return 0;
}


int genode_net_xmit_frame(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats = (struct net_device_stats*) netdev_priv(dev);
	int len                        = skb->len;
	void* addr                     = skb->data;

	/* collect acknowledgements of old packets */
	while (genode_net_tx_ack_avail())
		genode_net_tx_ack();

	/* transmit to nic-session */
	while (genode_net_tx(addr, len)) {
		/* tx queue is  full, could not enqueue packet */
		genode_net_tx_ack();
	}
	dev_kfree_skb(skb);

	/* save timestamp */
	dev->trans_start = jiffies;

	stats->tx_packets++;
	stats->tx_bytes += len;

	return 0;
}


struct net_device_stats* genode_net_get_stats(struct net_device *dev)
{
	return (struct net_device_stats*) netdev_priv(dev);
}


void genode_net_tx_timeout(struct net_device *dev)
{
}


static irqreturn_t event_interrupt(int irq, void *data)
{
	genode_net_rx_receive();
	return IRQ_HANDLED;
}


/**************************
 **  De-/Initialization  **
 **************************/

static const struct net_device_ops genode_net_dev_ops =
{
	.ndo_open       = genode_net_open,
	.ndo_stop       = genode_net_close,
	.ndo_start_xmit = genode_net_xmit_frame,
	.ndo_get_stats  = genode_net_get_stats,
	.ndo_tx_timeout = genode_net_tx_timeout
};

/* Setup and register the device. */
static int __init genode_net_init(void)
{
	int          err = 0;
	unsigned     irq;
	l4_cap_idx_t irq_cap;

	if (!genode_net_ready())
		return 0;

	/* allocate network device */
	if (!(net_dev = alloc_etherdev(sizeof(struct net_device_stats))))
		goto out;

	net_dev->netdev_ops      = &genode_net_dev_ops;
	net_dev->watchdog_timeo  = 20 * HZ;

	/* set MAC address */
	genode_net_mac(net_dev->dev_addr, ETH_ALEN);

	/**
	 * Obtain an IRQ for the device.
	 */
	irq_cap = genode_net_irq_cap();
	if ((irq = l4x_register_irq(irq_cap)) < 0)
		return -ENOMEM;
	if ((err = request_irq(irq, event_interrupt, IRQF_SAMPLE_RANDOM,
	                       "Genode net", net_dev))) {
		printk(KERN_WARNING "%s: request_irq failed: %d\n", __func__, err);
		return err;
	}

	/* register network device */
	if ((err = register_netdev(net_dev))) {
		panic("loopback: Failed to register netdevice: %d\n", err);
		goto out_free;
	}

	return 0;

out_free:
	free_netdev(net_dev);
out:
	return err;
};


static void __exit genode_net_exit(void)
{
	unregister_netdev(net_dev);
	free_netdev(net_dev);
}


module_init(genode_net_init);
module_exit(genode_net_exit);

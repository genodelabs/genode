/*
 * \brief  Back-end driver for IP stack
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2013-09-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux includes */
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

/* local includes */
#include <lx_emul.h>
#include <nic.h>


static struct net_device *_dev;


static int driver_net_open(struct net_device *dev)
{
	_dev = dev;
	return 0;
}


static int driver_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats = (struct net_device_stats*) netdev_priv(dev);
	int len                        = skb->len;
	void* addr                     = skb->data;

	/* transmit to nic-session */
	if (net_tx(addr, len)) {
		/* tx queue is  full, could not enqueue packet */
		pr_debug("TX packet dropped\n");
		return NETDEV_TX_BUSY;
	}

	dev_kfree_skb(skb);

	/* save timestamp */
	dev->trans_start = jiffies;

	stats->tx_packets++;
	stats->tx_bytes += len;

 	return NETDEV_TX_OK;
}


static int driver_change_mtu(struct net_device *dev, int new_mtu)
{
	/* possible point to reflect successful MTU setting */
	return eth_change_mtu(dev, new_mtu);
}


static const struct net_device_ops driver_net_ops =
{
	.ndo_open       = driver_net_open,
	.ndo_start_xmit = driver_net_xmit,
	.ndo_change_mtu = driver_change_mtu,
};


static int driver_init(void)
{
	struct net_device *dev;
	int err = -ENODEV;

	dev = alloc_etherdev(0);

	if (!(dev = alloc_etherdev(0)))
		goto out;

	dev->netdev_ops = &driver_net_ops;

	/* set MAC */
	net_mac(dev->dev_addr, ETH_ALEN);

	if ((err = register_netdev(dev))) {
		panic("driver: Could not register back-end %d\n", err);
		goto out_free;
	}

	return 0;

out_free:
	free_netdev(dev);
out:
	return err;
}


module_init(driver_init);


/**
 * Called by Nic_client when a packet was received
 */
void net_driver_rx(void *addr, unsigned long size)
{
	struct net_device_stats *stats;

	if (!_dev)
		return;

	stats = (struct net_device_stats*) netdev_priv(_dev);

	/* allocate skb */
	enum {
		ADDITIONAL_HEADROOM = 4, /* smallest value found by trial & error */
	};
	struct sk_buff *skb = netdev_alloc_skb_ip_align(_dev, size + ADDITIONAL_HEADROOM);
	if (!skb) {
		printk(KERN_NOTICE "genode_net_rx: low on mem - packet dropped!\n");
		stats->rx_dropped++;
		return;
	}

	/* copy packet */
	memcpy(skb_put(skb, size), addr, size);

	skb->dev       = _dev;
	skb->protocol  = eth_type_trans(skb, _dev);
	skb->ip_summed = CHECKSUM_NONE;

	netif_receive_skb(skb);

	stats->rx_packets++;
	stats->rx_bytes += size;
}

/**
 * \brief  Nic client that transfers packets from/to IP stack to/from nic client
 *         C-API
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <genode_c_api/nic_client.h>


static struct genode_nic_client *dev_nic_client(struct net_device *dev)
{
	return (struct genode_nic_client *)dev->ifalias;
}


static int net_open(struct net_device *dev)
{
	return 0;
}


struct genode_nic_client_tx_packet_context
{
	struct sk_buff *skb;
};


static unsigned long nic_tx_packet_content(struct genode_nic_client_tx_packet_context *ctx,
                                           char *dst, unsigned long dst_len)
{
	struct sk_buff * const skb = ctx->skb;

	if (dst_len < skb->len) {
		printk("nic_tx_packet_content: packet exceeds uplink packet size\n");
		memset(dst, 0, dst_len);
		return 0;
	}

	skb_copy_from_linear_data(skb, dst, skb->len);

	/* clear unused part of the destination buffer */
	memset(dst + skb->len, 0, dst_len - skb->len);

	return skb->len;
}


static int driver_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	bool progress = false;
	struct net_device_stats *stats = (struct net_device_stats*) netdev_priv(dev);

	struct genode_nic_client *nic_client = dev_nic_client(dev);
	struct genode_nic_client_tx_packet_context ctx = { .skb = skb };

	if (!nic_client) return NETDEV_TX_BUSY;

	progress = genode_nic_client_tx_packet(nic_client, nic_tx_packet_content, &ctx);
	/* transmit to nic-session */
	if (!progress) {
		/* tx queue is  full, could not enqueue packet */
		return NETDEV_TX_BUSY;
	}

	dev_kfree_skb(skb);

	/* save timestamp */
	netif_trans_update(dev);

	stats->tx_packets++;
	stats->tx_bytes += skb->len;

	genode_nic_client_notify_peers();

	return NETDEV_TX_OK;
}


static const struct net_device_ops net_ops =
{
	.ndo_open       = net_open,
	.ndo_start_xmit = driver_net_xmit,
};


static struct task_struct *nic_rx_task_struct_ptr;

struct genode_nic_client_rx_context
{
	struct net_device *dev;
};


struct task_struct *lx_nic_client_rx_task(void)
{
	return nic_rx_task_struct_ptr;
}


static genode_nic_client_rx_result_t nic_rx_one_packet(struct genode_nic_client_rx_context *ctx,
                                                       char const *ptr, unsigned long len)
{
	enum {
		ADDITIONAL_HEADROOM = 4, /* smallest value found by trial & error */
	};
	struct sk_buff *skb = netdev_alloc_skb_ip_align(ctx->dev, len + ADDITIONAL_HEADROOM);
	struct net_device_stats *stats = netdev_priv(ctx->dev);

	if (!skb) {
		printk("alloc_skb failed\n");
		return GENODE_NIC_CLIENT_RX_RETRY;
	}

	skb_copy_to_linear_data(skb, ptr, len);
	skb_put(skb, len);
	skb->dev       = ctx->dev;
	skb->protocol  = eth_type_trans(skb, ctx->dev);
	skb->ip_summed = CHECKSUM_NONE;

	netif_receive_skb(skb);

	stats->rx_packets++;
	stats->rx_bytes += len;

	return GENODE_NIC_CLIENT_RX_ACCEPTED;
}


static int rx_task_function(void *arg)
{
	struct net_device *dev = arg;
	struct genode_nic_client *nic_client = dev_nic_client(dev);
	struct genode_nic_client_rx_context ctx = { .dev = dev };

	while (true) {

		bool progress = false;

		lx_emul_task_schedule(true);

		while (genode_nic_client_rx(nic_client,
		                        nic_rx_one_packet,
		                        &ctx)) {
			progress = true; }

		if (progress) genode_nic_client_notify_peers();
	}

	return 0;
}


static int __init virtio_net_driver_init(void)
{
	struct net_device *dev;
	int err = -ENODEV;
	struct genode_mac_address mac;
	pid_t pid;

	dev = alloc_etherdev(0);

	if (!(dev = alloc_etherdev(0)))
		goto out;

	dev->netdev_ops = &net_ops;

	dev->ifalias = (struct dev_ifalias *)genode_nic_client_create("");

	if (!dev->ifalias) {
		printk("Failed to create nic client\n");
		goto out_free;
	}

	/* set MAC */
	mac = genode_nic_client_mac_address(dev_nic_client(dev));
	dev_addr_set(dev, mac.addr);

	if ((err = register_netdev(dev))) {
		printk("Could not register net device driver %d\n", err);
		goto out_nic;
	}

	/* create RX task */
	pid = kernel_thread(rx_task_function, dev, "rx_task", CLONE_FS | CLONE_FILES);

	nic_rx_task_struct_ptr = find_task_by_pid_ns(pid, NULL);

	return 0;

out_nic:
	genode_nic_client_destroy(dev_nic_client(dev));
out_free:
	free_netdev(dev);
out:
	return err;
}


/**
 * Let's hook into the virtio_net_driver initcall, so we do not need to register
 * an additional one
 */
module_init(virtio_net_driver_init);


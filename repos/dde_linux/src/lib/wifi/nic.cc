/*
 * \brief  Glue code for Linux network drivers
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2012-07-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/rpc_server.h>
#include <base/snprintf.h>
#include <cap_session/connection.h>
#include <nic/xml_node.h>
#include <nic_session/nic_session.h>
#include <os/config.h>
#include <util/xml_node.h>

/* local includes */
#include <lx.h>
#include <nic/component.h>

#include <extern_c_begin.h>
# include <lx_emul.h>
# include <net/cfg80211.h>
#include <extern_c_end.h>


enum {
	HEAD_ROOM = 128, /* XXX guessed value but works */
};

/**
 * Net_device to session glue code
 */
class Nic_device : public Nic::Device
{
	public: /* FIXME */

		struct net_device      *_ndev;
		Nic::Session_component *_session  = nullptr;
		bool                    _has_link = false;

	public:

		Nic_device(struct net_device *ndev) : _ndev(ndev) { }

		void rx(sk_buff *skb)
		{
			/* get mac header back */
			skb_push(skb, ETH_HLEN);

			void *packet       = skb->data;
			size_t packet_size = ETH_HLEN;
			void *frag         = 0;
			size_t frag_size   = 0;

			/**
			 * If received packets are too large (as of now 128 bytes) the actually
			 * payload is put into a fragment. Otherwise the payload is stored directly
			 * in the sk_buff.
			 */
			if (skb_shinfo(skb)->nr_frags) {
				if (skb_shinfo(skb)->nr_frags > 1)
					PERR("more than 1 fragment in skb");

				skb_frag_t *f = &skb_shinfo(skb)->frags[0];
				frag          = skb_frag_address(f);
				frag_size     = skb_frag_size(f);
			}
			else
				packet_size += skb->len;

			_session->rx((Genode::addr_t)packet, packet_size, (Genode::addr_t)frag, frag_size);
		}

		/**
		 * Report link state
		 */
		void link_state(bool link)
		{
			/* only report changes of the link state */
			if (link == _has_link)
				return;

			_has_link = link;

			if (_session)
				_session->link_state_changed();
		}

		/**********************
		 ** Device interface **
		 **********************/

		void session(Nic::Session_component *s) override { _session = s; }

		Nic::Mac_address mac_address() override
		{
			Nic::Mac_address m;
			Genode::memcpy(&m, _ndev->perm_addr, ETH_ALEN);
			return m;
		}

		bool link_state() override { return _has_link; }

		bool tx(Genode::addr_t virt, Genode::size_t size) override
		{
			struct sk_buff *skb = ::alloc_skb(size + HEAD_ROOM, GFP_KERNEL);
			skb_reserve(skb, HEAD_ROOM);

			unsigned char *data = skb_put(skb, size);
			Genode::memcpy(data, (void*)virt, size);

			_ndev->netdev_ops->ndo_start_xmit(skb, _ndev);
			return true;
		}
};


static Nic_device *_nic = 0;

static Server::Entrypoint *_ep;

void Lx::nic_init(Server::Entrypoint &ep) {
	_ep = &ep; }


void Lx::get_mac_address(unsigned char *addr)
{
	Genode::memcpy(addr, _nic->_ndev->perm_addr, ETH_ALEN);
}


namespace Lx {
	class Notifier;
}

class Lx::Notifier
{
	private:

		struct Block : public Genode::List<Block>::Element
		{
			struct notifier_block *nb;

			Block(struct notifier_block *nb) : nb(nb) { }
		};

		Lx::List<Block> _list;
		Genode::Tslab<Block, 32 * sizeof(Block)> _block_alloc;

	public:

		Notifier() : _block_alloc(Genode::env()->heap()) { }

		void register_block(struct notifier_block *nb)
		{
			Block *b = new (&_block_alloc) Block(nb);
			_list.insert(b);
		}

		void unregister_block(struct notifier_block *nb)
		{
			for (Block *b = _list.first(); b; b = b->next())
				if (b->nb == nb) {
					_list.remove(b);
					destroy(&_block_alloc, b);
					break;
				}
		}

		int call_all_blocks(unsigned long val, void *v)
		{
			int rv = NOTIFY_DONE;
			for (Block *b = _list.first(); b; b = b->next()) {
				rv = b->nb->notifier_call(b->nb, val, v);
				if (rv & NOTIFY_STOP_MASK)
					break;
			}
			return rv;
		}
};


/* XXX move blocking_notifier_call to proper location */
/**********************
 ** linux/notifier.h **
 **********************/

static Lx::Notifier &blocking_notifier()
{
	static Lx::Notifier inst;
	return inst;
}


int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                          struct notifier_block *nb)
{
	blocking_notifier().register_block(nb);
	return 0;
}

int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
                                          struct notifier_block *nb)
{
	blocking_notifier().unregister_block(nb);
	return 0;
}


int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long val, void *v)
{
	return blocking_notifier().call_all_blocks(val, v);
}


/***********************
 ** linux/netdevice.h **
 ***********************/

static Lx::Notifier &net_notifier()
{
	static Lx::Notifier inst;
	return inst;
}


extern "C" int register_netdevice_notifier(struct notifier_block *nb)
{
	/**
	 * In Linux the nb is actually called upon on registration. We do not
	 * that semantic because we add a net_device only after all notifiers
	 * were registered.
	 */
	net_notifier().register_block(nb);

	return 0;
}


extern "C" int unregster_netdevice_notifier(struct notifier_block *nb)
{
	net_notifier().unregister_block(nb);

	return 0;
}


extern "C" struct net_device * netdev_notifier_info_to_dev(struct netdev_notifier_info *info)
{
	/* we always pass a net_device pointer to this function */
	return reinterpret_cast<net_device *>(info);
}


struct Proto_hook : public Lx::List<Proto_hook>::Element
{
	struct packet_type &pt;

	Proto_hook(struct packet_type *pt) : pt(*pt) { }
};


class Proto_hook_list
{
	private:

		Lx::List<Proto_hook>  _list;
		Genode::Allocator    &_alloc;

	public:

		Proto_hook_list(Genode::Allocator &alloc) : _alloc(alloc) { }

		void insert(struct packet_type *pt) {
			_list.insert(new (&_alloc) Proto_hook(pt)); }

		void remove(struct packet_type *pt)
		{
			for (Proto_hook *ph = _list.first(); ph; ph = ph->next())
				if (&ph->pt == pt) {
					_list.remove(ph);
					destroy(&_alloc, ph);
					break;
				}
		}

		Proto_hook* first() { return _list.first(); }
};


static Proto_hook_list& proto_hook_list()
{
	static Proto_hook_list inst(*Genode::env()->heap());
	return inst;
}


extern "C" void dev_add_pack(struct packet_type *pt)
{
	proto_hook_list().insert(pt);
}


extern "C" void __dev_remove_pack(struct packet_type *pt)
{
	proto_hook_list().remove(pt);
}


extern "C" struct net_device *__dev_get_by_index(struct net *net, int ifindex)
{
	if (!_nic || !_nic->_ndev) {
		PERR("no net device registered!");
		return 0;
	}

	return _nic->_ndev;
}


extern "C" struct net_device *dev_get_by_index(struct net *net, int ifindex)
{
	return __dev_get_by_index(net, ifindex);
}


extern "C" int dev_hard_header(struct sk_buff *skb, struct net_device *dev,
                               unsigned short type, const void *daddr,
                               const void *saddr, unsigned int len)
{
	if (!dev->header_ops || !dev->header_ops->create)
		return 0;

	return dev->header_ops->create(skb, dev, type, daddr, saddr, len);
}


extern "C" int dev_parse_header(const struct sk_buff *skb, unsigned char *haddr)
{
	struct net_device const *dev = skb->dev;

	if (!dev->header_ops || dev->header_ops->parse)
		return 0;

	return dev->header_ops->parse(skb, haddr);
}


extern "C" int dev_queue_xmit(struct sk_buff *skb)
{
	struct net_device *dev           = skb->dev;
	struct net_device_ops const *ops = dev->netdev_ops;
	int rv = NETDEV_TX_OK;

	if (skb->next)
		PWRN("more skb's queued");

	rv = ops->ndo_start_xmit(skb, dev);

	return rv;
}


extern "C" size_t LL_RESERVED_SPACE(struct net_device *dev)
{
	return dev->hard_header_len ?
	       (dev->hard_header_len + (HH_DATA_MOD - 1)) & ~(HH_DATA_MOD - 1) : 0;
}


extern "C" int register_netdevice(struct net_device *ndev)
{
	static bool already_registered = false;

	if (already_registered) {
		PERR("We don't support multiple network devices in one driver instance");
		return -ENODEV;
	}

	static Nic_device nic_device(ndev);
	static Nic::Root  nic_root(*_ep, Genode::env()->heap(), nic_device);

	/*
	 * XXX This is just a starting point for removing all the static stuff from
	 * this file...
	 */
	ndev->lx_nic_device = (void *)&nic_device;
	_nic                =         &nic_device;

	already_registered = true;

	ndev->state |= 1UL << __LINK_STATE_START;
	netif_carrier_off(ndev);

	/* execute all notifier blocks */
	net_notifier().call_all_blocks(NETDEV_REGISTER, ndev);
	net_notifier().call_all_blocks(NETDEV_UP, ndev);
	ndev->ifindex = 1;

	/* set mac adress */
	Genode::memcpy(ndev->perm_addr, ndev->ieee80211_ptr->wiphy->perm_addr, ETH_ALEN);

	int err = ndev->netdev_ops->ndo_open(ndev);
	if (err) {
		PERR("ndev->netdev_ops->ndo_open(ndev): %d", err);
		return err;
	}

	if (ndev->netdev_ops->ndo_set_rx_mode)
		ndev->netdev_ops->ndo_set_rx_mode(ndev);

	Genode::env()->parent()->announce(_ep->rpc_ep().manage(&nic_root));

	list_add_tail_rcu(&ndev->dev_list, &init_net.dev_base_head);

	return 0;
}


extern "C" int netif_running(const struct net_device *dev)
{
	return dev->state & (1UL << __LINK_STATE_START);
}


extern "C" int netif_device_present(struct net_device *dev) { return 1; }


extern "C" int netif_carrier_ok(const struct net_device *dev)
{
	return !(dev->state & (1UL << __LINK_STATE_NOCARRIER));
}


extern "C" void netif_carrier_on(struct net_device *dev)
{
	dev->state &= ~(1UL << __LINK_STATE_NOCARRIER);

	Nic_device *nic = (Nic_device *)dev->lx_nic_device;

	nic->link_state(true);
}


extern "C" void netif_carrier_off(struct net_device *dev)
{
	if (!dev)
		PERR("!dev %p", __builtin_return_address(0));

	dev->state |= 1UL << __LINK_STATE_NOCARRIER;

	Nic_device *nic = (Nic_device *)dev->lx_nic_device;

	nic->link_state(false);
}


extern "C" int netif_receive_skb(struct sk_buff *skb)
{
	/**
	 * XXX check original linux implementation if it is really
	 * necessary to free the skb if it was not handled.
	 */

	/* send EAPOL related frames only to the wpa_supplicant */
	if (ntohs(skb->protocol) == ETH_P_PAE) {
		/* XXX call only AF_PACKET hook */
		for (Proto_hook* ph = proto_hook_list().first(); ph; ph = ph->next()) {
			ph->pt.func(skb, _nic->_ndev, &ph->pt, _nic->_ndev);
		}
		return NET_RX_SUCCESS;
	}

	if (_nic && _nic->_session) {
		_nic->rx(skb);
	}

	dev_kfree_skb(skb);
	return NET_RX_SUCCESS;
}


extern "C" u16 netdev_cap_txqueue(struct net_device *dev, u16 queue_index)
{
	if (queue_index > dev-> real_num_tx_queues) {
		PERR("error: queue_index %d out of range (%d max) called from: %p",
		     queue_index, dev->real_num_tx_queues, __builtin_return_address(0));
		return 0;
	}

	return queue_index;
}


extern "C" struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name,
                                    void (*setup)(struct net_device *),
                                    unsigned int txqs, unsigned int rxqs)
{
	struct net_device *dev;
	size_t alloc_size;
	struct net_device *p;

	alloc_size = ALIGN(sizeof(struct net_device), NETDEV_ALIGN);
	/* ensure 32-byte alignment of whole construct */
	alloc_size += NETDEV_ALIGN - 1;

	p = (net_device *)kzalloc(alloc_size, GFP_KERNEL | __GFP_NOWARN | __GFP_REPEAT);
	if (!p)
		return NULL;

	dev = PTR_ALIGN(p, NETDEV_ALIGN);

	dev->gso_max_size = GSO_MAX_SIZE;
	dev->gso_max_segs = GSO_MAX_SEGS;

	setup(dev);

	/* actually set by dev_open() */
	dev->flags |= IFF_UP;


	/* XXX our dev is always called wlan0 */
	strcpy(dev->name, "wlan0");

	dev->dev_addr = (unsigned char *)kzalloc(ETH_ALEN, GFP_KERNEL);
	if (!dev->dev_addr)
		return 0;

	if (sizeof_priv) {
		/* ensure 32-byte alignment of private area */
		dev->priv = kzalloc(sizeof_priv, GFP_KERNEL);
		if (!dev->priv)
			return 0;
	}

	dev->num_tx_queues = txqs;
	dev->real_num_tx_queues = txqs;
	struct netdev_queue *tx = (struct netdev_queue*)
	                           kcalloc(txqs, sizeof(struct netdev_queue),
	                                   GFP_KERNEL | GFP_LX_DMA);
	if (!tx) {
		PERR("could not allocate ndev_queues");
	}

	dev->_tx = tx;
	for (unsigned i = 0; i < txqs; i++) {
		tx[i].dev = dev;
		tx[i].numa_node = NUMA_NO_NODE;
	}

	return dev;
}


/**********************
 ** linux/inerrupt.h **
 **********************/



/*************************
 ** linux/etherdevice.h **
 *************************/

int is_valid_ether_addr(const u8 *addr)
{
	/* is multicast */
	if ((addr[0] & 0x1))
		return 0;

	/* zero */
	if (!(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]))
		return 0;

	return 1;
}

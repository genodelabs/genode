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
#include <os/config.h>
#include <nic/component.h>
#include <root/component.h>
#include <util/xml_node.h>

/* local includes */
#include <lx.h>

#include <extern_c_begin.h>
#include <lx_emul.h>
#include <net/cfg80211.h>
#include <extern_c_end.h>

extern bool config_verbose;

enum {
	HEAD_ROOM = 128, /* XXX guessed value but works */
};

/**
 * Nic::Session implementation
 */
class Wifi_session_component : public Nic::Session_component
{
	private:

		net_device *_ndev;
		bool        _has_link = !(_ndev->state & 1UL << __LINK_STATE_NOCARRIER);

	protected:

		bool _send()
		{
			using namespace Genode;

			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.valid()) {
				PWRN("Invalid tx packet");
				return true;
			}

			struct sk_buff *skb = ::alloc_skb(packet.size() + HEAD_ROOM, GFP_KERNEL);
			skb_reserve(skb, HEAD_ROOM);

			unsigned char *data = skb_put(skb, packet.size());
			Genode::memcpy(data, _tx.sink()->packet_content(packet), packet.size());

			_ndev->netdev_ops->ndo_start_xmit(skb, _ndev);
			_tx.sink()->acknowledge_packet(packet);

			return true;
		}

		void _handle_packet_stream()
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			while (_send()) ;
		}

	public:

		Wifi_session_component(Genode::size_t const tx_buf_size,
		                       Genode::size_t const rx_buf_size,
		                       Genode::Allocator   &rx_block_md_alloc,
		                       Genode::Ram_session &ram_session,
		                       Server::Entrypoint  &ep, net_device *ndev)
		: Session_component(tx_buf_size, rx_buf_size, rx_block_md_alloc, ram_session, ep),
		  _ndev(ndev)
		{
			_ndev->lx_nic_device = this;
		}

		~Wifi_session_component()
		{
			_ndev->lx_nic_device = nullptr;
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
			_link_state_changed();
		}

		void receive(struct sk_buff *skb)
		{
			_handle_packet_stream();

			if (!_rx.source()->ready_to_submit())
				return;

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


			try {
				Nic::Packet_descriptor p = _rx.source()->alloc_packet(packet_size + frag_size);
				void *buffer = _rx.source()->packet_content(p);
				memcpy(buffer, packet, packet_size);
				if (frag_size)
					memcpy((char *)buffer + packet_size, frag, frag_size);

				_rx.source()->submit_packet(p);
			} catch (...) {
				PDBG("failed to process received packet");
			}
		}


		/*****************************
		 ** NIC-component interface **
		 *****************************/

		Nic::Mac_address mac_address() override
		{
			Nic::Mac_address m;
			Genode::memcpy(&m, _ndev->perm_addr, ETH_ALEN);
			return m;
		}

		bool link_state() override { return _has_link; }
};


/**
 * NIC root implementation
 */
class Root : public Genode::Root_component<Wifi_session_component,
                                           Genode::Single_client>
{
	private:

		Server::Entrypoint &_ep;

	protected:

		Wifi_session_component *_create_session(const char *args)
		{
			using namespace Genode;

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (unsigned long)sizeof(Wifi_session_component));
			if (ram_quota < session_size)
				throw Genode::Root::Quota_exceeded();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers. Also check both sizes separately to handle a
			 * possible overflow of the sum of both sizes.
			 */
			if (tx_buf_size               > ram_quota - session_size
			 || rx_buf_size               > ram_quota - session_size
			 || tx_buf_size + rx_buf_size > ram_quota - session_size) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, tx_buf_size + rx_buf_size + session_size);
				throw Genode::Root::Quota_exceeded();
			}

			session = new (md_alloc())
			          Wifi_session_component(tx_buf_size, rx_buf_size,
			                                *env()->heap(),
			                                *env()->ram_session(),
			                                _ep, device);
			return session;
		}

	public:

		net_device             *device  = nullptr;
		Wifi_session_component *session = nullptr;
		static Root            *instance;

		Root(Server::Entrypoint &ep, Genode::Allocator &md_alloc)
		: Genode::Root_component<Wifi_session_component, Genode::Single_client>(&ep.rpc_ep(), &md_alloc),
			_ep(ep)
		{ }

		void announce() { Genode::env()->parent()->announce(_ep.rpc_ep().manage(this)); }
};


Root *Root::instance;


void Lx::nic_init(Server::Entrypoint &ep)
{
	static Root root(ep, *Genode::env()->heap());
	Root::instance = &root;
}


void Lx::get_mac_address(unsigned char *addr)
{
	memcpy(addr, Root::instance->device->perm_addr, ETH_ALEN);
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
	if (!Root::instance->device) {
		PERR("no net device registered!");
		return 0;
	}

	return Root::instance->device;
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

	already_registered = true;

	Root::instance->device = ndev;

	ndev->state |= 1UL << __LINK_STATE_START;
	netif_carrier_off(ndev);

	/* execute all notifier blocks */
	net_notifier().call_all_blocks(NETDEV_REGISTER, ndev);
	net_notifier().call_all_blocks(NETDEV_UP, ndev);
	ndev->ifindex = 1;

	/* set mac adress */
	Genode::memcpy(ndev->perm_addr, ndev->ieee80211_ptr->wiphy->perm_addr, ETH_ALEN);

	if (config_verbose)
		PINF("mac_address: %02x:%02x:%02x:%02x:%02x:%02x",
		     ndev->perm_addr[0], ndev->perm_addr[1], ndev->perm_addr[2],
		     ndev->perm_addr[3], ndev->perm_addr[4], ndev->perm_addr[5]);

	int err = ndev->netdev_ops->ndo_open(ndev);
	if (err) {
		PERR("ndev->netdev_ops->ndo_open(ndev): %d", err);
		return err;
	}

	if (ndev->netdev_ops->ndo_set_rx_mode)
		ndev->netdev_ops->ndo_set_rx_mode(ndev);

	Root::instance->announce();

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

	Wifi_session_component *session = (Wifi_session_component *)dev->lx_nic_device;

	if (session)
		session->link_state(true);
}


extern "C" void netif_carrier_off(struct net_device *dev)
{
	if (!dev)
		PERR("!dev %p", __builtin_return_address(0));

	dev->state |= 1UL << __LINK_STATE_NOCARRIER;

	Wifi_session_component *session = (Wifi_session_component *)dev->lx_nic_device;

	if (session)
		session->link_state(false);
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
			ph->pt.func(skb, Root::instance->device, &ph->pt, Root::instance->device);
		}
		return NET_RX_SUCCESS;
	}

	if (Root::instance->session)
		Root::instance->session->receive(skb);

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

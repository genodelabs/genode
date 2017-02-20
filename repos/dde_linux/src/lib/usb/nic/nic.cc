/*
 * \brief  Glue code for Linux network drivers
 * \author Sebastian Sumpf
 * \date   2012-07-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/rpc_server.h>
#include <base/snprintf.h>
#include <nic_session/nic_session.h>
#include <nic/xml_node.h>
#include <util/xml_node.h>

#include <lx_emul.h>
#include <lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>
#include <lx_emul/extern_c_end.h>

#include <lx_kit/env.h>
#include <lx_kit/malloc.h>

#include <usb_nic_component.h>
#include "signal.h"


static Signal_helper *_signal = 0;

enum {
	HEAD_ROOM = 8,  /* head room in skb in bytes */
	MAC_LEN   = 17,  /* 12 number and 6 colons */ 
};


/**
 * Internal alloc function
 */
struct sk_buff *_alloc_skb(unsigned int size, bool tx = true);


/**
 * Skb-bitmap allocator
 */
class Skb
{
	private:

		unsigned const _entries;

		sk_buff  *_buf;
		unsigned *_free;
		unsigned _idx;

		enum { ENTRY_ELEMENT_SIZE = sizeof(unsigned) * 8 };

	public:

		Skb(unsigned const entries, unsigned const buffer_size)
		:
			_entries(entries), _idx(0)
		{
			unsigned const size = _entries / sizeof(unsigned);

			_buf  = (sk_buff *)kmalloc(sizeof(sk_buff) * _entries, GFP_KERNEL);
			_free = (unsigned *)kmalloc(sizeof(unsigned) * size, GFP_KERNEL);

			Genode::memset(_free, 0xff, size * sizeof(unsigned));

			for (unsigned i = 0; i < _entries; i++)
				_buf[i].start = (unsigned char *)kmalloc(buffer_size + NET_IP_ALIGN, GFP_NOIO);
		}

		sk_buff *alloc()
		{
			unsigned const IDX = _entries / ENTRY_ELEMENT_SIZE;

			for (register unsigned i = 0; i < IDX; i++) {
				if (_free[_idx] != 0) {
					unsigned msb = Genode::log2(_free[_idx]);
					_free[_idx] ^= (1 << msb);

					sk_buff *r = &_buf[(_idx * ENTRY_ELEMENT_SIZE) + msb];
					r->data = r->start;
					r->phys   = 0;
					r->cloned = 0;
					r->clone  = 0;
					r->len    = 0;
					return r;
				}
				_idx = (_idx + 1) % IDX;
			}
			

			return 0;
		}

		void free(sk_buff *buf)
		{
			unsigned entry = buf - &_buf[0];
			if (&_buf[0] >  buf || entry > _entries)
				return;

			_idx = entry / ENTRY_ELEMENT_SIZE;
			_free[_idx] |= (1 << (entry % ENTRY_ELEMENT_SIZE));
		}
};


/* send/receive skb allocators */
static Skb *skb_tx(unsigned const elements = 0, unsigned const buffer_size = 0)
{
	static Skb _skb(elements, buffer_size);
	return &_skb;
}


static Skb *skb_rx(unsigned const elements = 0, unsigned const buffer_size = 0)
{
	static Skb _skb(elements, buffer_size);
	return &_skb;
}


/**
 * Prototype of fixup function
 */
extern "C" {
typedef struct sk_buff* (*fixup_t)(struct usbnet *, struct sk_buff *, gfp_t);
}


/**
 * Net_device to session glue code
 */
class Nic_device : public Usb_nic::Device
{
	public:

		struct net_device *_ndev;  /* Linux-net device */
		fixup_t            _tx_fixup;
		bool const         _burst;
		bool               _has_link { false };

	public:

		Nic_device(struct net_device *ndev)
		:
			_ndev(ndev),
			/* XXX should be configurable instead of guessing burst mode */
			_burst(((usbnet *)netdev_priv(ndev))->rx_urb_size > 2048)
		{
			struct usbnet *dev = (usbnet *)netdev_priv(_ndev);

			/* initialize skb allocators */
			unsigned urb_cnt = dev->rx_urb_size <= 2048 ? 128 : 64;
			skb_rx(urb_cnt, dev->rx_urb_size);
			skb_tx(urb_cnt, dev->rx_urb_size);

			if (!burst()) return;

			/*
			 * Retrieve 'tx_fixup' function from driver and set it to zero,
			 * so it cannot be called by the actual driver. Required for
			 * burst mode.
			 */
			_tx_fixup = dev->driver_info->tx_fixup;
			dev->driver_info->tx_fixup = 0;
		}

		/**
		 * Add device
		 */
		static Nic_device *add(struct net_device *ndev) {
			return new (Lx::Malloc::mem()) Nic_device(ndev); }

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

		bool link_state() override { return _has_link; }

		/**
		 * Submit packet to driver
		 */
		bool tx(Genode::addr_t virt, Genode::size_t size)
		{
			sk_buff *skb;
			
			if (!(skb = _alloc_skb(size + HEAD_ROOM)))
				return false;

			skb->len     = size;
			skb->data   += HEAD_ROOM;
			Genode::memcpy(skb->data, (void *)virt, skb->len);

			tx_skb(skb);
			return true;
		}

		/**
		 * Alloc an SKB
		 */
		sk_buff *alloc_skb()
		{
			struct usbnet *dev = (usbnet *)netdev_priv(_ndev);
			sk_buff *skb;

			if (!(skb = _alloc_skb(dev->rx_urb_size)))
				return 0;

			skb->len = 0;
			return skb;
		}

		/**
		 * Submit SKB to the driver
		 */
		void tx_skb(sk_buff *skb)
		{
			struct usbnet *dev = (usbnet *)netdev_priv(_ndev);
			unsigned long dropped = dev->net->stats.tx_dropped;
			_ndev->netdev_ops->ndo_start_xmit(skb, _ndev); 

			if (dropped < dev->net->stats.tx_dropped)
				Genode::warning("Dropped SKB");
		}

		/**
		 * Call tx_fixup function of driver
		 */
		void tx_fixup(struct sk_buff *skb)
		{
			struct usbnet *dev = (usbnet *)netdev_priv(_ndev);
			if(!_tx_fixup || !_tx_fixup(dev, skb, 0))
				Genode::error("Tx fixup error");
		}


		/**
		 * Fill an SKB with 'data' if 'size', return false if SKB is greater than
		 * 'end'
		 */
		bool skb_fill(struct sk_buff *skb, unsigned char *data, Genode::size_t size, unsigned char *end)
		{
			Genode::addr_t align = ((Genode::addr_t)(data + 3) & ~3);
			skb->truesize = skb->data == 0 ? 0 : (unsigned char*)align - data;
			data          = skb->data == 0 ? data : (unsigned char*)align;

			skb->start     = data;
			data          += HEAD_ROOM;
			skb->len       = size;
			skb->data      = data;
			skb->end       = skb->tail = data + size;
			skb->truesize += (skb->end - skb->start);


			return skb->end >= end ? false : true;
		}

		/**
		 * Submit packet for session
		 */
		inline void rx(sk_buff *skb) { _session->rx((Genode::addr_t)skb->data, skb->len); }

		/**
		 * Return mac address
		 */
		Nic::Mac_address mac_address()
		{
			Nic::Mac_address m;
			Genode::memcpy(&m, _ndev->_dev_addr, ETH_ALEN);
			return m;
		}

		bool burst() { return _burst; }
};


/* XXX support multiple devices */
static Nic_device *_nic = 0;


void Nic::init(Genode::Env &env) {
	_signal = new (Lx::Malloc::mem()) Signal_helper(env); }


/***********************
 ** linux/netdevice.h **
 ***********************/

int register_netdev(struct net_device *ndev)
{
	using namespace Genode;
	static bool announce = false;
	int err = -ENODEV;

	Nic_device *nic = Nic_device::add(ndev);

	/* XXX: move to 'main' */
	if (!announce) {
		static ::Root root(_signal->env(), Lx::Malloc::mem(), nic);

		announce = true;

		ndev->state |= 1 << __LINK_STATE_START;
		netif_carrier_off(ndev);

		if ((err = ndev->netdev_ops->ndo_open(ndev)))
			return err;

		if (ndev->netdev_ops->ndo_set_rx_mode)
			ndev->netdev_ops->ndo_set_rx_mode(ndev);

		_nic = nic;
		_signal->parent().announce(_signal->ep().rpc_ep().manage(&root));
	}

	return err;
}


int netif_running(const struct net_device *dev)
{
	return dev->state & (1 << __LINK_STATE_START);
}


int netif_device_present(struct net_device *dev) { return 1; }


int netif_carrier_ok(const struct net_device *dev)
{
	return !(dev->state & (1 << __LINK_STATE_NOCARRIER));
}


void netif_carrier_on(struct net_device *dev)
{
	dev->state &= ~(1 << __LINK_STATE_NOCARRIER);
	if (_nic)
		_nic->link_state(true);
}


void netif_carrier_off(struct net_device *dev)
{
	dev->state |= 1 << __LINK_STATE_NOCARRIER;
	if (_nic)
		_nic->link_state(false);
}

#ifdef GENODE_NET_STAT
	#include <nic/stat.h>
	static Timer::Connection _timer;
	static Nic::Measurement  _stat(_timer);
#endif

int netif_rx(struct sk_buff *skb)
{
	if (_nic && _nic->session()) {
		_nic->rx(skb);
	}
#ifdef GENODE_NET_STAT
	else if (_nic) {
		try {
		_stat.data(new (skb->data) Net::Ethernet_frame(skb->len), skb->len);
		} catch(Net::Ethernet_frame::No_ethernet_frame) {
			Genode::warning("No ether frame");
		}
	}
#endif

	dev_kfree_skb(skb);
	return NET_RX_SUCCESS;
}


/********************
 ** linux/skbuff.h **
 ********************/

struct sk_buff *_alloc_skb(unsigned int size, bool tx)
{
	sk_buff *skb = tx ?  skb_tx()->alloc() : skb_rx()->alloc();

	if (!skb)
		return 0;

	size = (size + 3) & ~(0x3);

	skb->end = skb->start + size;
	skb->tail = skb->start;
	skb->truesize = size;

	return skb;
}


struct sk_buff *alloc_skb(unsigned int size, gfp_t priority)
{
	/*
	 * Note: This is only called for RX skb's by the driver
	 */
	struct sk_buff *skb = _alloc_skb(size, false);
	return skb;
}

struct sk_buff *netdev_alloc_skb_ip_align(struct net_device *dev, unsigned int length)
{
	struct sk_buff *s = _alloc_skb(length + NET_IP_ALIGN, false);
	if (s && dev->net_ip_align) {
		s->data += NET_IP_ALIGN;
		s->tail += NET_IP_ALIGN;
	}
	return s;
}


void dev_kfree_skb(struct sk_buff *skb)
{
	lx_log(DEBUG_SKB, "free skb: %p start: %p cloned: %d",
	       skb, skb->start, skb->cloned);

	if (skb->cloned) {
		skb->start = skb->clone;
		skb->cloned = false;
		skb_rx()->free(skb);
		return;
	}

	skb_tx()->free(skb);
	skb_rx()->free(skb);
}


void dev_kfree_skb_any(struct sk_buff *skb) { dev_kfree_skb(skb); }

void kfree_skb(struct sk_buff *skb) { dev_kfree_skb(skb); }


/**
 * Reserve 'len'
 */
void skb_reserve(struct sk_buff *skb, int len)
{
	if ((skb->data + len) > skb->end) {
		Genode::error("Error resevring SKB data: skb: ", skb,  " data: ", skb->data,
		              " end: ", skb->end,  "len: ", skb->len);
		return;
	}
	skb->data += len;
	lx_log(DEBUG_SKB, "skb: %p slen: %u len: %d", skb, skb->len, len);
}


/**
 * Prepend 'len'
 */
unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
	if((skb->data - len) < skb->start) {
		Genode::error("Error SKB head room too small: ", skb, " data: ", skb->data,
		              " start: ", skb->start, " len: ", len);
		return 0;
	}

	skb->len  += len;
	skb->data -= len;

	lx_log(DEBUG_SKB, "skb: %p slen: %u len: %u", skb, skb->len, len);
	return skb->data;
}


/**
 * Append 'len'
 */
unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	if ((skb->data + len > skb->end)) {
		Genode::error("Error increasing SKB length: skb: ", skb, " data: ", skb->data,
		              " end: ", skb->end,  " len: ", len);
		return 0;
	}

	unsigned char *old = skb_tail_pointer(skb);
	skb->len  += len;
	skb->tail += len;
	lx_log(DEBUG_SKB, "skb: %p slen: %u len: %u", skb, skb->len, len);
	return old;
}


/**
 * Return current head room
 */
unsigned int skb_headroom(const struct sk_buff *skb) 
{
	return skb->data - skb->start;
}


int skb_tailroom(const struct sk_buff *skb)
{
	return skb->end - skb->tail; 
}



/**
 * Take 'len' from front
 */
unsigned char *skb_pull(struct sk_buff *skb, unsigned int len)
{
	if (len > skb->len) {
		Genode::error("Error try to pull too much: skb: ", skb, " len: ", skb->len,
		              " pull len: ", len);
		return 0;
	}
	skb->len -= len;
	lx_log(DEBUG_SKB, "skb: %p slen: %u len: %u", skb, skb->len, len);
	return skb->data += len;
}


/**
 * Set 'len' and 'tail'
 */
void skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (skb->len <= len) {
		Genode::error("Error trimming to ", len, " bytes skb: ", skb, " data: ",
		              skb->data, "  start: ", skb->start,  " len ", skb->len);
		return;
	}

	skb->len = len;
	skb_set_tail_pointer(skb, len);

	lx_log(DEBUG_SKB, "skb: %p slen: %u len: %u", skb, skb->len, len);
}


/**
 * Clone skb
 */
struct sk_buff *skb_clone(struct sk_buff *skb, gfp_t gfp_mask)
{
	sk_buff *c;

	if (!(c = alloc_skb(0,0)))
		return 0;

	unsigned char *start =  c->start;
	*c = *skb;

	/* save old start pointer */
	c->cloned = 1;
	c->clone  = start;
	return c;
}


int skb_header_cloned(const struct sk_buff *skb)
{
	return skb->cloned;
}


void skb_set_tail_pointer(struct sk_buff *skb, const int offset)
{
	skb->tail = skb->data + offset;
}


unsigned char *skb_tail_pointer(const struct sk_buff *skb)
{
	return skb->tail;
}


/**
 * Dummy for shared info
 */
struct skb_shared_info *skb_shinfo(struct sk_buff const * /* skb */)
{
	static skb_shared_info _s = { 0 };
	return &_s;
}


/**
 * Init list head
 */
void skb_queue_head_init(struct sk_buff_head *list)
{
	static int count_x = 0;
	list->prev = list->next = (sk_buff *)list;
	list->qlen = 0;
}

/**
 * Add to tail of queue
 */
void __skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	newsk->next      = (sk_buff *)list;
	newsk->prev      = list->prev;
	list->prev->next = newsk;
	list->prev       = newsk;
	list->qlen++;
}


void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk) {
	__skb_queue_tail(list, newsk); }


/**
 * Remove skb from queue
 */
void __skb_unlink(struct sk_buff *skb, struct sk_buff_head *list)
{
	if (!list->qlen)
		return;

	skb->prev->next = skb->next;
	skb->next->prev = skb->prev;
	skb->next = skb->prev = 0;
	list->qlen--;
}


/**
 * Remove from head of queue
 */
struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
	if (list->qlen == 0)
		return 0;
	
	sk_buff *skb = list->next;
	__skb_unlink(skb, list);

	return skb;
}


/**********************
 ** linux/inerrupt.h **
 **********************/

static void snprint_mac(u8 *buf, u8 *mac)
{
	for (int i = 0; i < ETH_ALEN; i++)
	{
		Genode::snprintf((char *)&buf[i * 3], 3, "%02x", mac[i]);
		if ((i * 3) < MAC_LEN)
			buf[(i * 3) + 2] = ':';
	}

	buf[MAC_LEN] = 0;
}


/*************************
 ** linux/etherdevice.h **
 *************************/

void eth_hw_addr_random(struct net_device *dev)
{
	random_ether_addr(dev->_dev_addr);
}


void eth_random_addr(u8 *addr)
{
	random_ether_addr(addr);
}


void random_ether_addr(u8 *addr)
{
	using namespace Genode;
	u8 str[MAC_LEN + 1];
	u8 fallback[] = { 0x2e, 0x60, 0x90, 0x0c, 0x4e, 0x01 };
	Nic::Mac_address mac;

	Xml_node config_node = Lx_kit::env().config_rom().xml();

	/* try using configured mac */
	try {
		Xml_node nic_config = config_node.sub_node("nic");
		Xml_node::Attribute mac_node = nic_config.attribute("mac");
		mac_node.value(&mac);
	} catch (...) {
	/* use fallback mac */
		snprint_mac(str, fallback);
		Genode::warning("No mac address or wrong format attribute in <nic> - using fallback (", str, ")");

		Genode::memcpy(addr, fallback, ETH_ALEN);
		return;
	}

	/* use configured mac*/
	Genode::memcpy(addr, mac.addr, ETH_ALEN);
	snprint_mac(str, (u8 *)mac.addr);
	Genode::log("Using configured mac: ", str);

#ifdef GENODE_NET_STAT
	_stat.set_mac(mac.addr);
#endif
}


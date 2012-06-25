/*
 * \brief  Glue code for Linux network drivers
 * \author Sebastian Sumpf
 * \date   2012-07-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/rpc_server.h>
#include <base/snprintf.h>
#include <nic_session/nic_session.h>
#include <cap_session/connection.h>
#include <os/config.h>
#include <util/xml_node.h>

#include <lx_emul.h>
#include <dma.h>

#include <nic/component.h>
#include "signal.h"

static Signal_helper *_signal = 0;

enum {
	START     = 0x1, /* device flag */
	HEAD_ROOM = 32,  /* head room in skb in bytes */
	MAC_LEN = 17,    /* 12 number and 6 colons */ 
};

class Nic_device : public Nic::Device
{
	private:

		struct net_device *_ndev; /* Linux-net device */

	public:

		Nic_device(struct net_device *ndev) : _ndev(ndev) { }

		/**
		 * Add device
		 */
		static Nic_device *add(struct net_device *ndev) {
			return new (Genode::env()->heap()) Nic_device(ndev); }


		/**********************
		 ** Device interface **
		 **********************/

		/**
		 * Submit packet to driver
		 */
		void tx(Genode::addr_t virt, Genode::size_t size)
		{
			sk_buff *skb = alloc_skb(size + HEAD_ROOM, 0);
			skb->len     = size;
			skb->data   += HEAD_ROOM;

			Genode::memcpy(skb->data, (void *)virt, skb->len);

			_ndev->netdev_ops->ndo_start_xmit(skb, _ndev);
		}

		/**
		 * Submit packet for session
		 */
		void rx(sk_buff *skb) { _session->rx((Genode::addr_t)skb->data, skb->len); }


		/**
		 * Return mac address
		 */
		Nic::Mac_address mac_address()
		{
			Nic::Mac_address m;
			Genode::memcpy(&m, _ndev->_dev_addr, ETH_ALEN);
			return m;
		}
};

/* XXX support multiple devices */
static Nic_device *_nic = 0;

void Nic::init(Genode::Signal_receiver *recv) {
	_signal = new (Genode::env()->heap()) Signal_helper(recv); }


/***********************
 ** linux/netdevice.h **
 ***********************/

int register_netdev(struct net_device *ndev)
{
	using namespace Genode;
	static bool announce = false;

	Nic_device *nic = Nic_device::add(ndev);

	/* XXX: move to 'main' */
	if (!announce) {
		static Cap_connection cap_nic;
		static Rpc_entrypoint ep_nic(&cap_nic, 4096, "usb_nic_ep");
		static Nic::Root root(&ep_nic, env()->heap(), _signal->receiver(), nic);

		announce = true;

		ndev->state |= START;
		int err = ndev->netdev_ops->ndo_open(ndev);
		_nic = nic;
		env()->parent()->announce(ep_nic.manage(&root));

		return err;
	}

	return -ENODEV;
}


int netif_running(const struct net_device *dev) { return dev->state & START; }
int netif_device_present(struct net_device *dev) { return 1; }


int netif_rx(struct sk_buff *skb)
{
	if (_nic)
		_nic->rx(skb);
	dev_kfree_skb(skb);
	return NET_RX_SUCCESS;
}


/********************
 ** linux/skbuff.h **
 ********************/

struct sk_buff *alloc_skb(unsigned int size, gfp_t priority)
{
	sk_buff *skb = new (Genode::env()->heap()) sk_buff;
	Genode::memset(skb, 0, sizeof(sk_buff));

	size = (size + 3) & ~(0x3);

	skb->start = skb->data = size ? (unsigned char*)kzalloc(size, 0) : 0;
	skb->tail  = skb->end = skb->start + size;
	skb->truesize = size;

	dde_kit_log(DEBUG_SKB, "alloc sbk: %p start: %p size: %u", skb, skb->start, size);
	return skb;
}


void dev_kfree_skb(struct sk_buff *skb)
{
	dde_kit_log(DEBUG_SKB, "free skb: %p start: %p cloned: %d",
	            skb, skb->start, skb->cloned);

	if (!skb->cloned)
		kfree(skb->start);

	destroy(Genode::env()->heap(), skb);
}


/**
 * Reserve 'len'
 */
void skb_reserve(struct sk_buff *skb, int len)
{
	if ((skb->data + len) > skb->end) {
		PERR("Error resevring SKB data: skb: %p data: %p end: %p len: %d",
		     skb, skb->data, skb->end, skb->len);
		return;
	}
	skb->data += len;
	dde_kit_log(DEBUG_SKB, "skb: %p slen: %u len: %d", skb, skb->len, len);
}


/**
 * Prepend 'len'
 */
unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
	if((skb->data - len) < skb->start) {
		PERR("Error SKB head room too small: %p data: %p start: %p len: %u",
		     skb, skb->data, skb->start, len);
		return 0;
	}

	skb->len  += len;
	skb->data -= len;

	dde_kit_log(DEBUG_SKB, "skb: %p slen: %u len: %u", skb, skb->len, len);
	return skb->data;
}


/**
 * Append 'len'
 */
unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	if ((skb->data + len > skb->end)) {
		PERR("Error increasing SKB length: skb: %p data: %p end: %p len: %u",
		      skb, skb->data, skb->end, len);
		return 0;
	}

	unsigned char *old = skb_tail_pointer(skb);
	skb->len  += len;
	skb->tail += len;
	dde_kit_log(DEBUG_SKB, "skb: %p slen: %u len: %u", skb, skb->len, len);
	return old;
}


/**
 * Return current head room
 */
unsigned int skb_headroom(const struct sk_buff *skb)
{
	return skb->data - skb->start;
}


/**
 * Take 'len' from front
 */
unsigned char *skb_pull(struct sk_buff *skb, unsigned int len)
{
	if (len > skb->len) {
		PERR("Error try to pull too much: skb: %p len: %u pull len: %u",
		     skb, skb->len, len);
		return 0;
	}
	skb->len -= len;
	dde_kit_log(DEBUG_SKB, "skb: %p slen: %u len: %u", skb, skb->len, len);
	return skb->data += len;
}


/**
 * Set 'len' and 'tail'
 */
void skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (skb->len <= len) {
		PERR("Error trimming skb: %p data: %p start: %p len %u ret: %p",
		     skb, skb->data, skb->start, len, __builtin_return_address((0)));
		return;
	}

	skb->len = len;
	skb_set_tail_pointer(skb, len);

	dde_kit_log(DEBUG_SKB, "skb: %p slen: %u len: %u", skb, skb->len, len);
}


/**
 * Clone skb
 */
struct sk_buff *skb_clone(struct sk_buff *skb, gfp_t gfp_mask)
{
	sk_buff *c = alloc_skb(0, 0);
	Genode::memcpy(c, skb, sizeof(sk_buff));
	c->cloned = 1;
	return c;
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
struct skb_shared_info *skb_shinfo(struct sk_buff * /* skb */)
{
	static skb_shared_info _s = { 0 };
	return &_s;
}


/**
 * Init list head
 */
void skb_queue_head_init(struct sk_buff_head *list)
{
	list->prev = list->next = (sk_buff *)list;
	list->qlen = 0;
}


/**
 * Add to tail of queue
 */
void __skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	newsk->next = (sk_buff *)list;
	newsk->prev  = list->prev;
	list->prev->next = newsk;
	list->prev = newsk;
	list->qlen++;
}


void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk) {
	__skb_queue_tail(list, newsk); }


/**
 * Remove skb from queue
 */
void __skb_unlink(struct sk_buff *skb, struct sk_buff_head *list)
{
	sk_buff *l = (sk_buff *)list;
	while (l->next != l) {
		l = l->next;

		if (l == skb) {
			l->prev->next = l->next;
			l->next->prev = l->prev;
			list->qlen--;
			return;
		}
	}

	PERR("SKB not found in __skb_unlink");
}


/**
 * Remove from head of queue
 */
struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
	if (list->next == (sk_buff *)list)
		return 0;

	sk_buff *skb = list->next;
	list->next = skb->next;
	list->next->prev = (sk_buff *)list;
	list->qlen--;

	return skb;
}


/**********************
 ** linux/inerrupt.h **
 **********************/

namespace Genode {

	/**
	 * Convert ASCII string to mac address
	 */
	template <>
	inline size_t ascii_to<Nic::Mac_address>(char const *s, Nic::Mac_address* mac, unsigned)
	{
		enum { 
			HEX     = true,
		};
	
		if(strlen(s) < MAC_LEN)
			throw -1;
	
		char mac_str[6];
		for (int i = 0; i < ETH_ALEN; i++) {
			int hi = i * 3;
			int lo = hi + 1;
	
			if (!is_digit(s[hi], HEX) || !is_digit(s[lo], HEX))
				throw -1;
	
			mac_str[i] = (digit(s[hi], HEX) << 4) | digit(s[lo], HEX);
		}
	
		memcpy(mac->addr, mac_str, ETH_ALEN);
	
		return MAC_LEN;
	}
}


static void snprint_mac(char *buf, char *mac)
{
	for (int i = 0; i < ETH_ALEN; i++)
	{
		Genode::snprintf(&buf[i * 3], 3, "%02x", mac[i]);
		if ((i * 3) < MAC_LEN)
			buf[(i * 3) + 2] = ':';
	}

	buf[MAC_LEN] = 0;
}


void random_ether_addr(u8 *addr)
{
	using namespace Genode;
	char str[MAC_LEN + 1];
	char fallback[] = { 0x2e, 0x60, 0x90, 0x0c, 0x4e, 0x01 };
	Nic::Mac_address mac;

	/* try using configured mac */
	try {
		Xml_node nic_config = config()->xml_node().sub_node("nic");
		Xml_node::Attribute mac_node = nic_config.attribute("mac");
		mac_node.value(&mac);
	} catch (...) {
		/* use fallback mac */
		snprint_mac(str, fallback);
		PWRN("No mac address or wrong format attribute in <nic> - using fallback (%s)",
		     str);

		Genode::memcpy(addr, fallback, ETH_ALEN);
		return;
	}

	/* use configured mac*/
	Genode::memcpy(addr, mac.addr, ETH_ALEN);
	snprint_mac(str, mac.addr);
	PINF("Using configured mac: %s", str);
}

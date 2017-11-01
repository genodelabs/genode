/*
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-03-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* linux includes */
#include <lx_emul.h>
#include <linux/skbuff.h>

/* local includes */
#include <lxc.h>


struct Skb skb_helper(struct sk_buff *skb)
{
	struct Skb helper;

	skb_push(skb, ETH_HLEN);

	helper.packet      = skb->data;
	helper.packet_size = ETH_HLEN;
	helper.frag        = 0;
	helper.frag_size   = 0;

	/**
	 * If received packets are too large (as of now 128 bytes) the actually
	 * payload is put into a fragment. Otherwise the payload is stored directly
	 * in the sk_buff.
	 */
	if (skb_shinfo(skb)->nr_frags) {
		if (skb_shinfo(skb)->nr_frags > 1)
			printk("more than 1 fragment in skb: %p nr_frags: %d", skb,
			       skb_shinfo(skb)->nr_frags);

		skb_frag_t *f    = &skb_shinfo(skb)->frags[0];
		helper.frag      = skb_frag_address(f);
		helper.frag_size = skb_frag_size(f);
	}
	else
		helper.packet_size += skb->len;

	return helper;
}


struct sk_buff *lxc_alloc_skb(size_t len, size_t headroom)
{
	struct sk_buff *skb = alloc_skb(len + headroom, GFP_KERNEL | GFP_LX_DMA);
	skb_reserve(skb, headroom);
	return skb;
}


unsigned char *lxc_skb_put(struct sk_buff *skb, size_t len)
{
	return skb_put(skb, len);
}

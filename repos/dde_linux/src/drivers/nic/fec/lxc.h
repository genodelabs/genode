/*
 * \brief  Lx C env
 * \author Josef Soentgen
 * \date   2016-03-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LXC_H_
#define _LXC_H_

/*
 * The sk_buff struct contains empty array members whose
 * size differs between C and C++. Since we want to access
 * certain members of the sk_buff from C++ we have to use
 * a uniform format useable from both languages.W
 *
 * Note: we pull struct skb_buff as well as size_t from
 * headers included before this one.
 */
struct Skb
{
	void   *packet;
	size_t  packet_size;
	void   *frag;
	size_t  frag_size;
};

struct Skb skb_helper(struct sk_buff *skb);
bool is_eapol(struct sk_buff *skb);

struct sk_buff *lxc_alloc_skb(size_t len, size_t headroom);
unsigned char *lxc_skb_put(struct sk_buff *skb, size_t len);

#endif /* _LXC_H_ */

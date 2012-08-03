/*
 * \brief  Test functions
 * \author Sebastian Sumpf
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#if 0

void tx_test() {
	char *data = (char *)skb->data;
	if (data[0x2a] == (char)0xaa && data[0x2b] == (char)0xbb) {
			PDBG("Got server signal");
			static char data[1066];
			static char header[] = {
			0x00, 0x1c, 0x25, 0x9e, 0x92, 0x4a, 0x2e, 0x60, 0x90, 0x0c, 0x4e, 0x01, 0x08, 0x00, 0x45, 0x00,
			0x04, 0x1c, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11, 0x22, 0x88, 0x0a, 0x00, 0x00, 0x3b, 0x0a, 0x00,
			0x00, 0x0f, 0x89, 0xc5, 0x04, 0xd2, 0x04, 0x08, 0x54, 0xfd};
			Genode::memset(data, 0, 1065);
			memcpy(data, header, sizeof(header));
			while (1) {
				sk_buff *skb = alloc_skb(1066 + HEAD_ROOM, 0);
				if (!skb) {
					Service_handler::s()->check_signal(true);
					continue;
				}
				skb->len     = 1066;
				skb->data   += HEAD_ROOM;

				memcpy(skb->data, data, 1066);

			_nic->_ndev->netdev_ops->ndo_start_xmit(skb, _nic->_ndev);
			dev_kfree_skb(skb);
			}
	}
}

#endif

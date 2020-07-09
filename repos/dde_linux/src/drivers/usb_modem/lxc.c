/*
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2014-03-07
 */

/*
 * Copyright (C) 2014-2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* linux includes */
#include <lx_emul.h>
#include <linux/skbuff.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/cdc.h>

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


/**
 * cdc_parse_cdc_header - parse the extra headers present in CDC devices
 * @hdr: the place to put the results of the parsing
 * @intf: the interface for which parsing is requested
 * @buffer: pointer to the extra headers to be parsed
 * @buflen: length of the extra headers
 *
 * This evaluates the extra headers present in CDC devices which
 * bind the interfaces for data and control and provide details
 * about the capabilities of the device.
 *
 * Return: number of descriptors parsed or -EINVAL
 * if the header is contradictory beyond salvage
 */

int cdc_parse_cdc_header(struct usb_cdc_parsed_header *hdr,
				struct usb_interface *intf,
				u8 *buffer,
				int buflen)
{
	/* duplicates are ignored */
	struct usb_cdc_union_desc *union_header = NULL;

	/* duplicates are not tolerated */
	struct usb_cdc_header_desc *header = NULL;
	struct usb_cdc_ether_desc *ether = NULL;
	struct usb_cdc_mdlm_detail_desc *detail = NULL;
	struct usb_cdc_mdlm_desc *desc = NULL;

	unsigned int elength;
	int cnt = 0;

	memset(hdr, 0x00, sizeof(struct usb_cdc_parsed_header));
	hdr->phonet_magic_present = false;
	while (buflen > 0) {
		elength = buffer[0];
		if (!elength) {
			dev_err(&intf->dev, "skipping garbage byte\n");
			elength = 1;
			goto next_desc;
		}
		if ((buflen < elength) || (elength < 3)) {
			dev_err(&intf->dev, "invalid descriptor buffer length\n");
			break;
		}
		if (buffer[1] != USB_DT_CS_INTERFACE) {
			dev_err(&intf->dev, "skipping garbage\n");
			goto next_desc;
		}

		switch (buffer[2]) {
		case USB_CDC_UNION_TYPE: /* we've found it */
			if (elength < sizeof(struct usb_cdc_union_desc))
				goto next_desc;
			if (union_header) {
				dev_err(&intf->dev, "More than one union descriptor, skipping ...\n");
				goto next_desc;
			}
			union_header = (struct usb_cdc_union_desc *)buffer;
			break;
		case USB_CDC_COUNTRY_TYPE:
			if (elength < sizeof(struct usb_cdc_country_functional_desc))
				goto next_desc;
			hdr->usb_cdc_country_functional_desc =
				(struct usb_cdc_country_functional_desc *)buffer;
			break;
		case USB_CDC_HEADER_TYPE:
			if (elength != sizeof(struct usb_cdc_header_desc))
				goto next_desc;
			if (header)
				return -EINVAL;
			header = (struct usb_cdc_header_desc *)buffer;
			break;
		case USB_CDC_ACM_TYPE:
			if (elength < sizeof(struct usb_cdc_acm_descriptor))
				goto next_desc;
			hdr->usb_cdc_acm_descriptor =
				(struct usb_cdc_acm_descriptor *)buffer;
			break;
		case USB_CDC_ETHERNET_TYPE:
			if (elength != sizeof(struct usb_cdc_ether_desc))
				goto next_desc;
			if (ether)
				return -EINVAL;
			ether = (struct usb_cdc_ether_desc *)buffer;
			break;
		case USB_CDC_CALL_MANAGEMENT_TYPE:
			if (elength < sizeof(struct usb_cdc_call_mgmt_descriptor))
				goto next_desc;
			hdr->usb_cdc_call_mgmt_descriptor =
				(struct usb_cdc_call_mgmt_descriptor *)buffer;
			break;
		case USB_CDC_DMM_TYPE:
			if (elength < sizeof(struct usb_cdc_dmm_desc))
				goto next_desc;
			hdr->usb_cdc_dmm_desc =
				(struct usb_cdc_dmm_desc *)buffer;
			break;
		case USB_CDC_MDLM_TYPE:
			if (elength < sizeof(struct usb_cdc_mdlm_desc *))
				goto next_desc;
			if (desc)
				return -EINVAL;
			desc = (struct usb_cdc_mdlm_desc *)buffer;
			break;
		case USB_CDC_MDLM_DETAIL_TYPE:
			if (elength < sizeof(struct usb_cdc_mdlm_detail_desc *))
				goto next_desc;
			if (detail)
				return -EINVAL;
			detail = (struct usb_cdc_mdlm_detail_desc *)buffer;
			break;
		case USB_CDC_NCM_TYPE:
			if (elength < sizeof(struct usb_cdc_ncm_desc))
				goto next_desc;
			hdr->usb_cdc_ncm_desc = (struct usb_cdc_ncm_desc *)buffer;
			break;
		case USB_CDC_MBIM_TYPE:
			if (elength < sizeof(struct usb_cdc_mbim_desc))
				goto next_desc;

			hdr->usb_cdc_mbim_desc = (struct usb_cdc_mbim_desc *)buffer;
			break;
		case USB_CDC_MBIM_EXTENDED_TYPE:
			if (elength < sizeof(struct usb_cdc_mbim_extended_desc))
				break;
			hdr->usb_cdc_mbim_extended_desc =
				(struct usb_cdc_mbim_extended_desc *)buffer;
			break;
		case CDC_PHONET_MAGIC_NUMBER:
			hdr->phonet_magic_present = true;
			break;
		default:
			/*
			 * there are LOTS more CDC descriptors that
			 * could legitimately be found here.
			 */
			dev_dbg(&intf->dev, "Ignoring descriptor: type %02x, length %ud\n",
					buffer[2], elength);
			goto next_desc;
		}
		cnt++;
next_desc:
		buflen -= elength;
		buffer += elength;
	}
	hdr->usb_cdc_union_desc = union_header;
	hdr->usb_cdc_header_desc = header;
	hdr->usb_cdc_mdlm_detail_desc = detail;
	hdr->usb_cdc_mdlm_desc = desc;
	hdr->usb_cdc_ether_desc = ether;
	return cnt;
}

struct usb_interface *usb_ifnum_to_if(const struct usb_device *dev,
                                      unsigned ifnum)
{
	struct usb_host_config *config = dev->actconfig;
	int i;

	if (!config) {
		lx_printf("No config for %u\n", ifnum);
		return NULL;
	}

	for (i = 0; i < config->desc.bNumInterfaces; i++) {
		if (config->interface[i]->altsetting[0]
				.desc.bInterfaceNumber == ifnum) {
			return config->interface[i];
		}
	}

	lx_printf("No interface for %u\n");
	return NULL;
}
EXPORT_SYMBOL(cdc_parse_cdc_header);

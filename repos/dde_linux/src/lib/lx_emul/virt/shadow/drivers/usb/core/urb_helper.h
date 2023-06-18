/*
 * \brief  URB handling helpers
 * \author Sebastian Sumpf
 * \date   2023-06-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/usb.h>
#include <genode_c_api/usb_client.h>


/*
 * Translate USB-client packet errors to errno
 */
static inline int packet_errno(int error)
{
	switch (error) {
	case INTERFACE_OR_ENDPOINT_ERROR: return -ENOENT;
	case MEMORY_ERROR:                return -ENOMEM;
	case NO_DEVICE_ERROR:             return -ESHUTDOWN;
	case PACKET_INVALID_ERROR:        return -EINVAL;
	case PROTOCOL_ERROR:              return -EPROTO;
	case STALL_ERROR:                 return -EPIPE;
	case TIMEOUT_ERROR:               return -ETIMEDOUT;
	case UNKNOWN_ERROR:
		printk("%s: got UNKNOWN_ERROR code\n", __func__);
		return -EIO;
	}

	return -EIO;
}


/*
 * Wait for a call to 'urb_destroy'
 */
int wait_for_free_urb(unsigned int timeout_jiffies);

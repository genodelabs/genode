/*
 * \brief  Complete initialization of msghdr
 * \author Christian Helmuth
 * \date   2017-05-29
 */

/*
 * Copyright (C) 202017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _MSGHDR_H_
#define _MSGHDR_H_

#include <legacy/lx_emul/extern_c_begin.h>
#include <linux/socket.h>
#include <legacy/lx_emul/extern_c_end.h>


static inline msghdr create_msghdr(void *name, int namelen, size_t datalen,
                                   struct iovec *iov)
{
	msghdr msg;

	msg.msg_name            = name;
	msg.msg_namelen         = namelen;
	msg.msg_iter.type       = 0;
	msg.msg_iter.iov_offset = 0;
	msg.msg_iter.count      = datalen;
	msg.msg_iter.iov        = iov;
	msg.msg_iter.nr_segs    = 1;
	msg.msg_control         = nullptr;
	msg.msg_controllen      = 0;
	msg.msg_flags           = 0;
	msg.msg_iocb            = nullptr;

	return msg;
}

#endif /* _MSGHDR_H_ */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska HÃ¶gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 /*
  * NOTE: @author : Aditya Kousik
  *       @email  : adit267.kousik@gmail.com
  *
  *       This implementation is for the libc-net port of Genode
  *
  */

#include "namespace.h"
#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <string.h>

#include <stddef.h>
#include "un-namespace.h"


ssize_t sendmsg(int s, const struct msghdr *msg, int flags)
{
	ssize_t ret;
	size_t tot = 0;
	int i;
	char *buf, *p;
	struct iovec *iov = msg->msg_iov;

	for(i = 0; i < msg->msg_iovlen; ++i)
		tot += iov[i].iov_len;
	buf = malloc(tot);
	if (tot != 0 && buf == NULL) {
		//errno = ENOMEM;
		return -1;
	}
	p = buf;
	for (i = 0; i < msg->msg_iovlen; ++i) {
		memcpy (p, iov[i].iov_base, iov[i].iov_len);
		p += iov[i].iov_len;
	}
	ret = _sendto(s, buf, tot, flags, msg->msg_name, msg->msg_namelen);
	free (buf);
	return ret;
}

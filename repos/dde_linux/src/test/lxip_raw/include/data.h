/**
 * \brief  Test data and helpers
 * \author Sebastian Sumpf
 * \date   2023-09-21
 *
 */

/*
 * Copyright (C) 2023-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _INCLUDE__DATA_H_
#define _INCLUDE__DATA_H_

namespace Test {

	enum { MAX_UDP_LOAD= 1472 };

	struct Msg_header
	{
		genode_iovec  iovec;
		genode_msghdr msg { };

		Msg_header(void const *data, unsigned long size)
		: iovec { const_cast<void *>(data), size }
		{
			msg.iov    = &iovec;
			msg.iovlen = 1;
		}

		Msg_header(genode_sockaddr &name, void const *data, unsigned long size)
		: Msg_header(data, size)
		{
			msg.name = &name;
		}

		genode_msghdr *header() { return &msg; }
	};

	struct Data
	{
		enum { SIZE = MAX_UDP_LOAD * 10 };

		char buf[SIZE];

		Data()
		{
			char j = 'A';
			unsigned step = MAX_UDP_LOAD / 2;
			for (unsigned i = 0; i < size(); i += step, j += 1) {
				Genode::memset(buf + i, j, step);
			}
		}

		char const *buffer() const { return buf; }
		unsigned long size() const { return SIZE; }
	};

	struct Http
	{
		Genode::String<128> header
			{ "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n" }; /* HTTP response header */ 

		Genode::String<150> html
			{"<html><head><title>Congrats!</title></head><body>"
			 "<h1>Welcome to our HTTP server!</h1>"
			 "<p>This is a small test page.</body></html>"}; /* HTML page */
	};

}

#endif /* _INCLUDE__DATA_H_ */

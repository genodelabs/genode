/*
 * \brief  HTTP back-end interface
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-08-24
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _HTTP_H_
#define _HTTP_H_

#include <base/stdint.h>

struct addrinfo;

using String = Genode::String<64>;
class Http
{
	typedef Genode::size_t size_t;
	typedef Genode::addr_t addr_t;
	typedef Genode::off_t  off_t;

	private:

		Genode::Heap   &_heap;
		size_t           _size;      /* number of bytes in file */
		char            *_host;      /* host name */
		char            *_port;      /* host port */
		char            *_path;      /* absolute file path on host */
		char            *_http_buf;  /* internal data buffer */
		unsigned         _http_ret;  /* HTTP status code */
		struct addrinfo *_info;      /* Resolved address info for host */
		int              _fd;        /* Socket file handle */
		addr_t          _base_addr; /* Address of I/O dataspace */

		/*
		 * Send 'HEAD' command
		 */
		void cmd_head();

		/*
		 * Connect to host
		 */
		void connect();

		/*
		 * Re-connect to host
		 */
		void reconnect();

		/*
		 * Set URI of remote file
		 */
		void parse_uri(::String const &uri);

		/*
		 * Resolve host
		 */
		void resolve_uri();

		/*
		 * Read HTTP header and parse server-status code
		 */
		size_t read_header();

		/*
		 * Determine remote-file size
		 */
		void get_capacity();

		/*
		 * Read 'size' bytes into buffer
		 */
		void do_read(void * buf, size_t size);

	public:

		/*
		 * Constructor (default host port is 80
		 */
		Http(Genode::Heap &heap, ::String const &uri);

		/*
		 * Destructor
		 */
		~Http();

		/**
		 * Read remote file size
		 *
		 * \return Remote file size in bytes
		 */
		size_t file_size() const { return _size; }

		/**
		 * Set base address of I/O dataspace
		 *
		 * \param base_addr  Base of dataspace
		 */
		void  base_addr(addr_t base_addr) { _base_addr = base_addr; }

		/**
		 * Send 'GET' command
		 *
		 * \param file_offset  Read from offset of remote file
		 * \param size         Number of byts to transfer
		 * \param buffer       address in I/O dataspace
		 */
		void cmd_get(size_t file_offset, size_t size, addr_t buffer);

		/* Exceptions */
		class Exception     : public ::Genode::Exception { };
		class Uri_error     : public Exception { };
		class Socket_error  : public Exception { };
		class Socket_closed : public Exception { };
		class Server_error  : public Exception { };
};

#endif /* _HTTP_H_ */

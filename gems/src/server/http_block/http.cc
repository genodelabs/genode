/*
 * \brief  HTTP protocol parts
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/child.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <lwip/genode.h>
#include <nic/packet_allocator.h>

extern "C" {
#include <lwip/netdb.h>
}

#include "http.h"

using namespace Genode;

/*
 * Debugging output
 */
static const int verbose = 0;

enum {
	/* HTTP status codes */
	HTTP_SUCC_OK      = 200,
	HTTP_SUCC_PARTIAL = 206,

	/* Size of our local buffer */
	HTTP_BUF = 2048,
};

/* Tokenizer policy */
struct Scanner_policy_file
{
	static bool identifier_char(char c, unsigned /* i */)
	{
		return c != ':' && c != 0 && c != ' ' && c != '\n';
	}
};

typedef ::Genode::Token<Scanner_policy_file> Http_token;


void Http::cmd_head()
{
	const char *http_templ = "%s %s HTTP/1.1\r\n"
	                         "Host: %s\r\n"
	                         "\r\n";

	int length = snprintf(_http_buf, HTTP_BUF, http_templ, "HEAD", _path, _host);

	if (lwip_write(_fd, _http_buf, length) != length) {
		PERR("Write error");
		throw Http::Socket_error();
	}
}


void Http::connect()
{
	_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
	if (_fd < 0) {
		PERR("No socket avaiable");
		throw Http::Socket_error();
	}

	if (lwip_connect(_fd, _info->ai_addr, sizeof(*(_info->ai_addr))) < 0) {
		PERR("Connect failed");
		throw Http::Socket_error();
	}
}


void Http::reconnect(){ lwip_close(_fd); connect(); }


void Http::resolve_uri()
{
	struct addrinfo *info;
	if (lwip_getaddrinfo(_host, _port, 0, &info)) {
		PERR("Error: Host %s not found", _host);
		throw Http::Uri_error();
	}

	env()->heap()->alloc(sizeof(struct addrinfo), &_info);
	Genode::memcpy(_info, info, sizeof(struct addrinfo));
}


Genode::size_t Http::read_header()
{
	bool header = true; size_t i = 0;

	while (header) {
		if (!lwip_read(_fd, &_http_buf[i], 1))
			throw Http::Socket_closed();

		/* DEBUG: Genode::printf("%c", _http_buf[i]); */

		if (i >= 3 && _http_buf[i - 3] == '\r' && _http_buf[i - 2] == '\n'
		 && _http_buf[i - 1] == '\r' && _http_buf[i - 0] == '\n')
			header = false;

		if (++i >= HTTP_BUF) {
			PERR("Buffer overflow");
			throw Http::Socket_error();
		}
	}

	/* scan for status code */
	Http_token t(_http_buf, i);
	for (int count = 0;; t = t.next()) {

		if (t.type() != Http_token::IDENT)
			continue;

		if (count) {
			ascii_to(t.start(), &_http_ret);
			break;
		}

		count++;
	}

	return i;
}


void Http::get_capacity()
{
	cmd_head();
	size_t  len = read_header();
	char buf[32];
	Http_token t(_http_buf, len);

	bool key = false;
	while (t) {

		if (t.type() != Http_token::IDENT) {
			t = t.next();
			continue;
		}

		if (key) {
			ascii_to(t.start(), &_size);

			if (verbose)
				PDBG("File size: %zu bytes", _size);

			break;
		}

		t.string(buf, 32);

		if (!Genode::strcmp(buf, "Content-Length", 32))
			key = true;

		t = t.next();
	}
}


void Http::do_read(void * buf, size_t size)
{
	size_t buf_fill = 0;

	while (buf_fill < size) {

		int part;
		if ((part = lwip_read(_fd, (void *)((addr_t)buf + buf_fill),
		                      size - buf_fill)) <= 0) {
			PERR("Error: Reading data (%d)", errno);
			throw Http::Socket_error();
		}

		buf_fill += part;
	}

	if (verbose)
		PDBG("Read %zu/%zu", buf_fill, size);
}


Http::Http(char *uri, size_t length) : _port((char *)"80")
{
	env()->heap()->alloc(HTTP_BUF, &_http_buf);

	/* parse URI */
	parse_uri(uri, length);

	/* search for host */
	resolve_uri();

	/* connect to host */
	connect();

	/* retrieve file info */
	get_capacity();
}


Http::~Http()
{
	env()->heap()->free(_host, Genode::strlen(_host) + 1);
	env()->heap()->free(_path, Genode::strlen(_path) + 2);
	env()->heap()->free(_http_buf, HTTP_BUF);
	env()->heap()->free(_info, sizeof(struct addrinfo));
}


void Http::parse_uri(char *uri, size_t length)
{
	/* strip possible http prefix */
	const char *http = "http://";
	size_t http_len = Genode::strlen(http);
	if (!strcmp(http, uri, http_len)) {
		uri    += http_len;
		length -= http_len;
	}

	/* set host and file path */
	size_t i;
	for (i = 0; i < length && uri[i] != '/'; i++) ;

	env()->heap()->alloc(i + 1, &_host);
	Genode::strncpy(_host, uri, i + 1);

	env()->heap()->alloc(length - i + 1, &_path);
	Genode::strncpy(_path, uri + i, length - i + 1);

	/* look for port */
	size_t len = Genode::strlen(_host);
	for (i = 0; i < len && _host[i] != ':'; i++) ;
	if (i < len) {
		_port =  &_host[i + 1];
		_host[i] = '\0';
	}

	if (verbose)
		PDBG("Port: %s", _port);

}


void Http::cmd_get(size_t file_offset, size_t size, off_t offset)
{
	if (verbose)
		PDBG("Read: offs %zu  size: %zu I/O offs: %lx", file_offset, size, offset);

	while (true) {

		const char *http_templ = "GET %s HTTP/1.1\r\n"
		                         "Host: %s\r\n"
		                         "Range: bytes=%lu-%lu\r\n"
		                         "\r\n";

		int length = snprintf(_http_buf, HTTP_BUF, http_templ, _path, _host,
		                      file_offset, file_offset + size - 1);

		if (lwip_write(_fd, _http_buf, length) < 0) {

			if (errno == ESHUTDOWN)
				reconnect();

			if (lwip_write(_fd, _http_buf, length) < 0)
				throw Http::Socket_error();
		}

		try {
			read_header();
		} catch (Http::Socket_closed) {
			reconnect();
			continue;
		}

		if (_http_ret != HTTP_SUCC_PARTIAL) {
			PERR("Error: Server returned %u", _http_ret);
			throw Http::Server_error();
		}

		do_read((void *)(_base_addr + offset), size);
		return;
	}
}


void __attribute__((constructor)) init()
{
	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	lwip_tcpip_init();

	if (lwip_nic_init(0, 0, 0, BUF_SIZE, BUF_SIZE)) {
		PERR("DHCP failed");
		throw -1;
	}
}


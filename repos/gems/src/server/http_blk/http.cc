/*
 * \brief  HTTP protocol parts
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/child.h>
#include <base/log.h>
#include <base/sleep.h>
#include <lwip_legacy/genode.h>
#include <nic/packet_allocator.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include "http.h"

using namespace Genode;

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

	if (write(_fd, _http_buf, length) != length) {
		error("cmd_head: write error");
		throw Http::Socket_error();
	}
}


void Http::connect()
{
	_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_fd < 0) {
		error("connect: no socket avaiable");
		throw Http::Socket_error();
	}

	if (::connect(_fd, _info->ai_addr, sizeof(*(_info->ai_addr))) < 0) {
		error("connect: connect failed");
		throw Http::Socket_error();
	}
}


void Http::reconnect(){ close(_fd); connect(); }


void Http::resolve_uri()
{
	struct addrinfo *info;
	if (getaddrinfo(_host, _port, 0, &info)) {
		error("host ", Cstring(_host), " not found");
		throw Http::Uri_error();
	}

	_heap.alloc(sizeof(struct addrinfo), (void**)&_info);
	Genode::memcpy(_info, info, sizeof(struct addrinfo));
}


Genode::size_t Http::read_header()
{
	bool header = true; size_t i = 0;

	while (header) {
		if (!read(_fd, &_http_buf[i], 1))
			throw Http::Socket_closed();

		if (i >= 3 && _http_buf[i - 3] == '\r' && _http_buf[i - 2] == '\n'
		 && _http_buf[i - 1] == '\r' && _http_buf[i - 0] == '\n')
			header = false;

		if (++i >= HTTP_BUF) {
			error("read_header: buffer overflow");
			throw Http::Socket_error();
		}
	}

	/* scan for status code */
	Http_token t(_http_buf, i);
	for (int count = 0;; t = t.next()) {

		if (t.type() != Http_token::IDENT)
			continue;

		if (count) {
			ascii_to(t.start(), _http_ret);
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
			ascii_to(t.start(), _size);
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
		if ((part = read(_fd, (void *)((addr_t)buf + buf_fill),
		                      size - buf_fill)) <= 0) {
			error("could not read data (", errno, ")");
			throw Http::Socket_error();
		}

		buf_fill += part;
	}
}


Http::Http(Genode::Heap &heap, ::String &uri)
: _heap(heap), _port((char *)"80")
{
	_heap.alloc(HTTP_BUF, (void**)&_http_buf);

	/* parse URI */
	parse_uri(uri);

	/* search for host */
	resolve_uri();

	/* connect to host */
	connect();

	/* retrieve file info */
	get_capacity();
}


Http::~Http()
{
	_heap.free(_host, Genode::strlen(_host) + 1);
	_heap.free(_path, Genode::strlen(_path) + 2);
	_heap.free(_http_buf, HTTP_BUF);
	_heap.free(_info, sizeof(struct addrinfo));
}


void Http::parse_uri(::String & u)
{
	/* strip possible http prefix */
	const char *http = "http://";
	char * uri = const_cast<char*>(u.string());
	size_t length   = Genode::strlen(uri);
	size_t http_len = Genode::strlen(http);
	if (!strcmp(http, uri, http_len)) {
		uri    += http_len;
		length -= http_len;
	}

	/* set host and file path */
	size_t i;
	for (i = 0; i < length && uri[i] != '/'; i++) ;

	_heap.alloc(i + 1, (void**)&_host);
	Genode::strncpy(_host, uri, i + 1);

	_heap.alloc(length - i + 1, (void**)&_path);
	Genode::strncpy(_path, uri + i, length - i + 1);

	/* look for port */
	size_t len = Genode::strlen(_host);
	for (i = 0; i < len && _host[i] != ':'; i++) ;
	if (i < len) {
		_port =  &_host[i + 1];
		_host[i] = '\0';
	}
}


void Http::cmd_get(size_t file_offset, size_t size, addr_t buffer)
{
	while (true) {

		const char *http_templ = "GET %s HTTP/1.1\r\n"
		                         "Host: %s\r\n"
		                         "Range: bytes=%lu-%lu\r\n"
		                         "\r\n";

		int length = snprintf(_http_buf, HTTP_BUF, http_templ, _path, _host,
		                      file_offset, file_offset + size - 1);

		if (write(_fd, _http_buf, length) < 0) {

			if (errno == ESHUTDOWN)
				reconnect();

			if (write(_fd, _http_buf, length) < 0)
				throw Http::Socket_error();
		}

		try {
			read_header();
		} catch (Http::Socket_closed) {
			reconnect();
			continue;
		}

		if (_http_ret != HTTP_SUCC_PARTIAL) {
			error("cmd_get: server returned ", _http_ret);
			throw Http::Server_error();
		}

		do_read((void *)(buffer), size);
		return;
	}
}

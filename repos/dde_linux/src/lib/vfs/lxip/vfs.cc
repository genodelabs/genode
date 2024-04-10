/*
 * \brief  lxip-based socket file system
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2016-02-01
 *
 * 2023-11-08: adjust to socket C-API
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <net/ipv4.h>
#include <util/string.h>
#include <util/xml_node.h>
#include <vfs/directory_service.h>
#include <vfs/file_io_service.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>
#include <timer_session/connection.h>

/* format-string includes */
#include <format/snprintf.h>

#include <genode_c_api/socket_types.h>
#include <genode_c_api/socket.h>

namespace {

using size_t = Genode::size_t;

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

	void name(genode_sockaddr &name)
	{
		msg.name =&name;
	}

	genode_msghdr *header() { return &msg; }
};


long get_port(char const *p)
{
	long tmp = -1;

	while (*++p) {
		if (*p == ':') {
			Genode::ascii_to_unsigned(++p, tmp, 10);
			break;
		}
	}
	return tmp;
}


unsigned get_addr(char const *p)
{
	unsigned char to[4] = { 0, 0, 0, 0};

	for (unsigned char &c : to) {

		unsigned result = 0;
		p += Genode::ascii_to_unsigned(p, result, 10);

		c = (unsigned char)result;

		if (*p == '.') ++p;
		if (*p == 0) break;
	};

	return (to[0]<<0)|(to[1]<<8)|(to[2]<<16)|(to[3]<<24);
}


long get_family(char const *p)
{
	long tmp = -1;

	while (*p) {
		if (*p == ';') {
			Genode::ascii_to_unsigned(++p, tmp, 1);
			break;
		}
		p++;
	}
	return tmp;
}

}


namespace Lxip {

	struct Protocol_dir;
	struct Socket_dir;

	class Protocol_dir_impl;

	enum {
		MAX_SOCKETS         = 128,       /* 3 */
		MAX_SOCKET_NAME_LEN = 3 + 1,     /* + \0 */
		MAX_DATA_LEN        = 32,        /* 255.255.255.255:65536 + something */
	};
}


namespace Vfs {

	using namespace Genode;

	struct Node;
	struct Directory;
	struct File;

	class Lxip_file;
	class Lxip_data_file;
	class Lxip_bind_file;
	class Lxip_accept_file;
	class Lxip_connect_file;
	class Lxip_listen_file;
	class Lxip_local_file;
	class Lxip_remote_file;
	class Lxip_peek_file;

	class Lxip_socket_dir;
	struct Lxip_socket_handle;

	class Lxip_link_state_file;
	class Lxip_address_file;

	struct Lxip_vfs_handle;
	class Lxip_vfs_file_handle;
	class Lxip_vfs_dir_handle;
	class Lxip_file_system;

	typedef Genode::List<List_element<Lxip_vfs_file_handle> > Lxip_vfs_file_handles;
}


/***************
 ** Vfs nodes **
 ***************/

struct Vfs::Node
{
	char const *_name;

	Node(char const *name) : _name(name) { }

	virtual ~Node() { }

	char const *name() { return _name; }

	virtual void close() { }

	Node(const Node&) = delete;
	Node operator=(const Node&) = delete;
};


struct Vfs::File : Vfs::Node
{
	Lxip_vfs_file_handles handles { };

	File(char const *name) : Node(name) { }

	virtual ~File() { }

	/**
	 * Read or write operation would block exception
	 */
	struct Would_block { };

	/**
	 * Check for data to read or write
	 */
	virtual bool poll() { return true; }

	virtual long write(Lxip_vfs_file_handle &,
	                   Const_byte_range_ptr const &, file_size)
	{
		Genode::error(name(), " not writeable");
		return -1;
	}

	virtual long read(Lxip_vfs_file_handle &,
	                  Byte_range_ptr const &, file_size)
	{
		Genode::error(name(), " not readable");
		return -1;
	}

	virtual File_io_service::Sync_result sync() {
		return File_io_service::Sync_result::SYNC_OK; }
};


struct Vfs::Directory : Vfs::Node
{
	Directory(char const *name) : Node(name) { }

	virtual ~Directory() { };

	virtual Vfs::Node *child(char const *)                        = 0;
	virtual file_size num_dirent()                                = 0;

	typedef Vfs::Directory_service::Open_result Open_result;
	virtual Open_result open(Vfs::File_system &fs,
	                         Genode::Allocator &alloc,
	                         char const*, unsigned, Vfs::Vfs_handle**) = 0;

	virtual long read(Byte_range_ptr const &, file_size seek_offset) = 0;
};


struct Lxip::Protocol_dir : Vfs::Directory
{
	enum Type { TYPE_STREAM, TYPE_DGRAM };

	virtual char const *top_dir() = 0;
	virtual Type type() = 0;
	virtual unsigned adopt_socket(Lxip::Socket_dir &) = 0;
	virtual bool     lookup_port(long) = 0;
	virtual void release(unsigned id) = 0;

	Protocol_dir(char const *name) : Vfs::Directory(name) { }
};


struct Lxip::Socket_dir : Vfs::Directory
{
	typedef Vfs::Directory_service::Open_result Open_result;

	virtual Protocol_dir &parent() = 0;
	virtual char const *top_dir() = 0;
	virtual void     bind(bool) = 0; /* bind to port */
	virtual long     bind() = 0; /* return bound port */
	virtual bool     lookup_port(long) = 0;
	virtual void     connect(bool) = 0;
	virtual void     listen(bool) = 0;
	virtual genode_sockaddr &remote_addr() = 0;
	virtual void     close() override = 0;
	virtual bool     closed() const = 0;

	Socket_dir(char const *name) : Vfs::Directory(name) { }
};


struct Vfs::Lxip_vfs_handle : Vfs::Vfs_handle
{
	typedef File_io_service:: Read_result Read_result;
	typedef File_io_service::Write_result Write_result;
	typedef File_io_service::Sync_result Sync_result;

	Lxip_vfs_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags)
	: Vfs::Vfs_handle(fs, fs, alloc, status_flags) { }

	/**
	 * Check if the file attached to this handle is ready to read
	 */
	virtual bool read_ready() const = 0;

	virtual Read_result   read(Byte_range_ptr       const &dst, size_t &out_count) = 0;
	virtual Write_result write(Const_byte_range_ptr const &src, size_t &out_count) = 0;

	virtual Sync_result sync() {
		return Sync_result::SYNC_OK; }
};


struct Vfs::Lxip_vfs_file_handle final : Vfs::Lxip_vfs_handle
{
	Lxip_vfs_file_handle(Lxip_vfs_file_handle const &);
	Lxip_vfs_file_handle &operator = (Lxip_vfs_file_handle const &);

	Vfs::File *file;

	/* file association element */
	List_element<Lxip_vfs_file_handle> file_le { this };

	/* notification elements */
	typedef Genode::Fifo_element<Lxip_vfs_file_handle> Fifo_element;
	typedef Genode::Fifo<Fifo_element> Fifo;

	Fifo_element read_ready_elem { *this };

	char content_buffer[Lxip::MAX_DATA_LEN];

	Lxip_vfs_file_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                     Vfs::File *file)
	: Lxip_vfs_handle(fs, alloc, status_flags), file(file)
	{
		if (file)
			file->handles.insert(&file_le);
	}

	~Lxip_vfs_file_handle()
	{
		if (file)
			file->handles.remove(&file_le);
	}

	bool read_ready() const override {
		return (file) ? file->poll() : false; }

	Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
	{
		if (!file) return Read_result::READ_ERR_INVALID;
		long res = file->read(*this, dst, seek());
		if (res < 0) return Read_result::READ_ERR_IO;
		out_count = res;
		return Read_result::READ_OK;
	}

	Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
	{
		if (!file) return Write_result::WRITE_ERR_INVALID;
		long res = file->write(*this, src, seek());
		if (res < 0) return Write_result::WRITE_ERR_IO;
		out_count = res;
		return Write_result::WRITE_OK;
	}

	bool write_content_line(Const_byte_range_ptr const &src)
	{
		if (src.num_bytes > sizeof(content_buffer) - 2)
			return false;

		Genode::memcpy(content_buffer, src.start, src.num_bytes);
		content_buffer[src.num_bytes + 0] = '\n';
		content_buffer[src.num_bytes + 1] = '\0';
		return true;
	}

	virtual Sync_result sync() override {
		return (file) ? file->sync() : Sync_result::SYNC_ERR_INVALID; }
};


struct Vfs::Lxip_vfs_dir_handle final : Vfs::Lxip_vfs_handle
{
	Vfs::Directory &dir;

	Lxip_vfs_dir_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                    Vfs::Directory &dir)
	: Vfs::Lxip_vfs_handle(fs, alloc, status_flags),
	  dir(dir) { }

	bool read_ready() const override { return true; }

	Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
	{
		long res = dir.read(dst, seek());
		if (res < 0) return Read_result::READ_ERR_IO;
		out_count = res;
		return Read_result::READ_OK;
	}

	Write_result write(Const_byte_range_ptr const &, size_t &) override {
		return Write_result::WRITE_ERR_INVALID; }
};


static Vfs::Lxip_vfs_file_handle::Fifo *_read_ready_waiters_ptr;

static void poll_all()
{
	_read_ready_waiters_ptr->for_each(
			[&] (Vfs::Lxip_vfs_file_handle::Fifo_element &elem) {
		Vfs::Lxip_vfs_file_handle &handle = elem.object();
		if (handle.file) {
			if (handle.file->poll()) {
				/* do not notify again until notify_read_ready */
				_read_ready_waiters_ptr->remove(elem);

				handle.read_ready_response();
			}
		}
	});
}

/*****************************
 ** Lxip vfs specific nodes **
 *****************************/

class Vfs::Lxip_file : public Vfs::File
{
	protected:

		Lxip::Socket_dir     &_parent;
		genode_socket_handle &_sock;

		Errno _write_err = GENODE_ENONE;

	public:

		Lxip_file(Lxip::Socket_dir &p, genode_socket_handle &s, char const *name)
		: Vfs::File(name), _parent(p), _sock(s) { }

		virtual ~Lxip_file() { }

		/**
		 * Dissolve relationship between handle and file, file and polling list.
		 */
		void dissolve_handles()
		{
			Genode::List_element<Vfs::Lxip_vfs_file_handle> *le = handles.first();
			while (le) {
				Vfs::Lxip_vfs_file_handle *h = le->object();
				handles.remove(&h->file_le);
				h->file = nullptr;
				le = handles.first();
			}
		}

		File_io_service::Sync_result sync() override
		{
			return (_write_err)
				? File_io_service::Sync_result::SYNC_ERR_INVALID
				: File_io_service::Sync_result::SYNC_OK;
		}
};


class Vfs::Lxip_data_file final : public Vfs::Lxip_file
{
	public:

		Lxip_data_file(Lxip::Socket_dir &p, genode_socket_handle &s)
		: Lxip_file(p, s, "data") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override
		{
			return genode_socket_poll(&_sock) & genode_socket_pollin_set();
		}

		long write(Lxip_vfs_file_handle &,
		           Const_byte_range_ptr const &src,
		           file_size /* ignored */) override
		{
			unsigned long bytes_sent = 0;
			Msg_header    msg_send { src.start, src.num_bytes };

			/* destination address is only required for UDP */
			if (_parent.parent().type() == Lxip::Protocol_dir::TYPE_DGRAM)
				msg_send.name(_parent.remote_addr());

			_write_err = genode_socket_sendmsg(&_sock, msg_send.header(), &bytes_sent);

			/* propagate EAGAIN */
			if (_write_err == GENODE_EAGAIN)
				throw Would_block();

			return _write_err == GENODE_ENONE ? bytes_sent : -1;
		}

		long read(Lxip_vfs_file_handle &,
		          Byte_range_ptr const &dst,
		          file_size /* ignored */) override
		{
			unsigned long bytes = 0;
			Msg_header    msg_recv { dst.start, dst.num_bytes };

			Errno err = genode_socket_recvmsg(&_sock, msg_recv.header(), &bytes, false);
			if (err == GENODE_EAGAIN)
				throw Would_block();

			return bytes;
		}
};


class Vfs::Lxip_peek_file final : public Vfs::Lxip_file
{
	public:

		Lxip_peek_file(Lxip::Socket_dir &p, genode_socket_handle &s)
		: Lxip_file(p, s, "peek") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override
		{
			/* can always peek */
			return true;
		}

		long write(Lxip_vfs_file_handle &,
		     Const_byte_range_ptr const &, file_size) override
		{
			return -1;
		}

		long read(Lxip_vfs_file_handle &,
		                   Byte_range_ptr const &dst,
		                   file_size /* ignored */) override
		{
			unsigned long bytes_avail = 0;
			Msg_header    msg_recv { dst.start, dst.num_bytes };

			Errno err = genode_socket_recvmsg(&_sock, msg_recv.header(), &bytes_avail, true);

			if (err == GENODE_EAGAIN)
				return -1;

			return bytes_avail;
		}
};


class Vfs::Lxip_bind_file final : public Vfs::Lxip_file
{
	private:

		long _port = -1;

	public:

		Lxip_bind_file(Lxip::Socket_dir &p, genode_socket_handle &s)
		: Lxip_file(p, s, "bind") { }

		long port() { return _port; }

		/********************
		 ** File interface **
		 ********************/

		long write(Lxip_vfs_file_handle &handle,
		           Const_byte_range_ptr const &src,
		           file_size /* ignored */) override
		{
			/* already bound to port */
			if (_port >= 0) return -1;

			if (!handle.write_content_line(src)) return -1;

			/* check if port is already used by other socket */
			long port = get_port(handle.content_buffer);
			if (port == -1)                return -1;
			if (_parent.lookup_port(port)) return -1;

			/* port is free, try to bind it */
			genode_sockaddr addr;
			addr.family  = AF_INET;
			addr.in.port = host_to_big_endian<genode_uint16_t>(uint16_t(port));
			addr.in.addr = get_addr(handle.content_buffer);

			_write_err = genode_socket_bind(&_sock, &addr);
			if (_write_err != GENODE_ENONE) return -1;

			_port = port;

			_parent.bind(true);

			return src.num_bytes;
		}

		long read(Lxip_vfs_file_handle &handle,
		          Byte_range_ptr const &dst,
		          file_size /* ignored */) override
		{
			if (dst.num_bytes < sizeof(handle.content_buffer))
				return -1;

			Genode::size_t const n = Genode::strlen(handle.content_buffer);
			Genode::memcpy(dst.start, handle.content_buffer, n);

			return n;
		}
};


class Vfs::Lxip_listen_file final : public Vfs::Lxip_file
{
	private:

		unsigned long _backlog = ~0UL;

	public:

		Lxip_listen_file(Lxip::Socket_dir &p, genode_socket_handle &s)
		: Lxip_file(p, s, "listen") { }

		/********************
		 ** File interface **
		 ********************/

		long write(Lxip_vfs_file_handle &handle,
		           Const_byte_range_ptr const &src,
		           file_size /* ignored */) override
		{
			/* write-once */
			if (_backlog != ~0UL) return -1;

			if (!handle.write_content_line(src)) return -1;

			Genode::ascii_to_unsigned(
				handle.content_buffer, _backlog, sizeof(handle.content_buffer));

			if (_backlog == ~0UL) return -1;

			_write_err = genode_socket_listen(&_sock, (int)_backlog);
			if (_write_err != GENODE_ENONE) {
				handle.write_content_line(Const_byte_range_ptr("", 0));
				return -1;
			}

			_parent.listen(true);

			return src.num_bytes;
		}

		long read(Lxip_vfs_file_handle &,
		          Byte_range_ptr const &dst,
		          file_size /* ignored */) override
		{
			return Format::snprintf(dst.start, dst.num_bytes, "%lu\n", _backlog);
		}
};


class Vfs::Lxip_connect_file final : public Vfs::Lxip_file
{
	private:

		bool _connecting   = false;
		bool _is_connected = false;

	public:

		Lxip_connect_file(Lxip::Socket_dir &p, genode_socket_handle &s)
		: Lxip_file(p, s, "connect") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override
		{
			/*
			 * The connect file is considered readable when the socket is
			 * writeable (connected or error).
			 */
			return genode_socket_poll(&_sock) & genode_socket_pollout_set();
		}

		long write(Lxip_vfs_file_handle &handle,
		           Const_byte_range_ptr const &src,
		           file_size /* ignored */) override
		{
			if (!handle.write_content_line(src)) return -1;

			long const port = get_port(handle.content_buffer);
			long const family = get_family(handle.content_buffer);
			if (port == -1) return -1;

			genode_sockaddr addr;
			addr.family  = family == 0 ? AF_UNSPEC : AF_INET;
			addr.in.port = host_to_big_endian<genode_uint16_t>(uint16_t(port));
			addr.in.addr = get_addr(handle.content_buffer);

			_write_err = genode_socket_connect(&_sock, &addr);

			switch (_write_err) {
			case GENODE_EINPROGRESS:
				_connecting = true;
				_write_err = GENODE_ENONE;
				return src.num_bytes;

			case GENODE_EALREADY:
				return -1;

			case GENODE_EISCONN:
				/*
				 * Connecting on an already connected socket is an error.
				 * If we get this error after we got EINPROGRESS it is
				 * fine.
				 */
				if (_is_connected || !_connecting) return -1;
				_is_connected = true;
				_write_err = GENODE_ENONE;
				break;

			default:
				if (_write_err != GENODE_ENONE) return -1;
				_is_connected = true;
				break;
			}

			genode_sockaddr &remote_addr = _parent.remote_addr();
			remote_addr.in.port          = host_to_big_endian<genode_uint16_t>(uint16_t(port));
			remote_addr.in.addr          = get_addr(handle.content_buffer);
			remote_addr.family           = AF_INET;

			_parent.connect(true);

			return src.num_bytes;
		}

		long read(Lxip_vfs_file_handle &,
		                   Byte_range_ptr const &dst,
		                   file_size /* ignored */) override
		{
			enum Errno socket_err, err;
			unsigned size = sizeof(enum Errno);
			err = genode_socket_getsockopt(&_sock, GENODE_SOL_SOCKET, GENODE_SO_ERROR,
			                               &socket_err, &size);


			if (err != GENODE_ENONE) {
				Genode::error("Vfs::Lxip_connect_file::read(): getsockopt() failed");
				return -1;
			}

			switch (socket_err) {
			case GENODE_ENONE:
				return Format::snprintf(dst.start, dst.num_bytes, "connected");
			case GENODE_ECONNREFUSED:
				return Format::snprintf(dst.start, dst.num_bytes, "connection refused");
			default:
				return Format::snprintf(dst.start, dst.num_bytes, "unknown error");
			}
		}
};


class Vfs::Lxip_local_file final : public Vfs::Lxip_file
{
	public:

		Lxip_local_file(Lxip::Socket_dir &p, genode_socket_handle &s)
		: Lxip_file(p, s, "local") { }

		/********************
		 ** File interface **
		 ********************/

		long read(Lxip_vfs_file_handle &handle,
		                   Byte_range_ptr const &dst,
		                   file_size /* ignored */) override
		{
			if (dst.num_bytes < sizeof(handle.content_buffer))
				return -1;

			genode_sockaddr addr;
			if (genode_socket_getsockname(&_sock, &addr) != GENODE_ENONE) return -1;

			unsigned char const *a = (unsigned char *)&addr.in.addr;
			unsigned char const *p = (unsigned char *)&addr.in.port;
			return Format::snprintf(dst.start, dst.num_bytes,
			                        "%d.%d.%d.%d:%u\n",
			                        a[0], a[1], a[2], a[3], (p[0]<<8)|(p[1]<<0));
		}
};


class Vfs::Lxip_remote_file final : public Vfs::Lxip_file
{
	public:

		Lxip_remote_file(Lxip::Socket_dir &p, genode_socket_handle &s)
		: Lxip_file(p, s, "remote") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override
		{
			switch (_parent.parent().type()) {
			case Lxip::Protocol_dir::TYPE_DGRAM:
				return genode_socket_poll(&_sock) & genode_socket_pollin_set();

			case Lxip::Protocol_dir::TYPE_STREAM:
				return true;
			}

			return false;
		}

		long read(Lxip_vfs_file_handle &handle,
		          Byte_range_ptr const &dst,
		          file_size /* ignored */) override
		{
			genode_sockaddr addr { .family = AF_INET };

			switch (_parent.parent().type()) {
			case Lxip::Protocol_dir::TYPE_DGRAM:
				{
					/* peek the sender address of the next packet */
					unsigned long bytes = 0;
					Msg_header msg_recv = { addr, handle.content_buffer, sizeof(handle.content_buffer) };

					Errno err = genode_socket_recvmsg(&_sock, msg_recv.header(), &bytes, true);
					if (err == GENODE_EAGAIN)
						throw Would_block();

					if (err) return -1;
				}
				break;
			case Lxip::Protocol_dir::TYPE_STREAM:
				{
					if (genode_socket_getpeername(&_sock, &addr) != GENODE_ENONE)
						return -1;
				}
				break;
			}

			unsigned char const *a = (unsigned char *)&addr.in.addr;
			unsigned char const *p = (unsigned char *)&addr.in.port;
			return Format::snprintf(dst.start, dst.num_bytes,
			                        "%d.%d.%d.%d:%u\n",
			                        a[0], a[1], a[2], a[3], (p[0]<<8)|(p[1]<<0));
		}

		long write(Lxip_vfs_file_handle &handle,
		           Const_byte_range_ptr const &src,
		           file_size /* ignored */) override
		{
			if (!handle.write_content_line(src)) return -1;

			long const port = get_port(handle.content_buffer);
			if (port == -1) return -1;

			genode_sockaddr &remote_addr = _parent.remote_addr();
			remote_addr.in.port          = host_to_big_endian<genode_uint16_t>(uint16_t(port));
			remote_addr.in.addr          = get_addr(handle.content_buffer);
			remote_addr.family           = AF_INET;

			return src.num_bytes;
		}
};


class Vfs::Lxip_accept_file final : public Vfs::Lxip_file
{
	public:

		Lxip_accept_file(Lxip::Socket_dir &p, genode_socket_handle &s)
		: Lxip_file(p, s, "accept") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override
		{
			return genode_socket_poll(&_sock) & genode_socket_pollin_set();
		}

		long read(Lxip_vfs_file_handle &,
		          Byte_range_ptr const &dst,
		          file_size /* ignored */) override
		{
			if (genode_socket_poll(&_sock) & genode_socket_pollin_set()) {
				copy_cstring(dst.start, "1\n", dst.num_bytes);
				return Genode::strlen(dst.start);
			}

			throw Would_block();
		}
};


class Vfs::Lxip_socket_dir final : public Lxip::Socket_dir
{
	public:

		enum {
			ACCEPT_NODE, BIND_NODE, CONNECT_NODE,
			DATA_NODE, PEEK_NODE,
			LOCAL_NODE, LISTEN_NODE, REMOTE_NODE,
			ACCEPT_SOCKET_NODE,
			MAX_FILES
		};

	private:

		Genode::Allocator        &_alloc;
		Lxip::Protocol_dir       &_parent;
		genode_socket_handle     &_sock;

		Vfs::File *_files[MAX_FILES];

		genode_sockaddr _remote_addr { };

		unsigned _num_nodes()
		{
			unsigned num = 0;
			for (Vfs::File *n : _files) num += (n != nullptr);
			return num;
		}

		Lxip_accept_file  _accept_file  { *this, _sock };
		Lxip_bind_file    _bind_file    { *this, _sock };
		Lxip_connect_file _connect_file { *this, _sock };
		Lxip_data_file    _data_file    { *this, _sock };
		Lxip_peek_file    _peek_file    { *this, _sock };
		Lxip_listen_file  _listen_file  { *this, _sock };
		Lxip_local_file   _local_file   { *this, _sock };
		Lxip_remote_file  _remote_file  { *this, _sock };

		struct Accept_socket_file : Vfs::File
		{
			Accept_socket_file() : Vfs::File("accept_socket") { }

		} _accept_socket_file { };

		char _name[Lxip::MAX_SOCKET_NAME_LEN];

		Vfs::Directory_service::Open_result
		_accept_new_socket(Vfs::File_system &fs,
		                   Genode::Allocator &alloc,
		                   Vfs::Vfs_handle **out_handle);

	public:

		unsigned const id;

		Lxip_socket_dir(Genode::Allocator &alloc,
		                Lxip::Protocol_dir &parent,
		                genode_socket_handle &sock)
		:
			Lxip::Socket_dir(_name),
			_alloc(alloc), _parent(parent),
			_sock(sock), id(parent.adopt_socket(*this))
		{
			Format::snprintf(_name, sizeof(_name), "%u", id);

			for (Vfs::File * &file : _files) file = nullptr;

			_files[ACCEPT_NODE]  = &_accept_file;
			_files[BIND_NODE]    = &_bind_file;
			_files[CONNECT_NODE] = &_connect_file;
			_files[DATA_NODE]    = &_data_file;
			_files[PEEK_NODE]    = &_peek_file;
			_files[LISTEN_NODE]  = &_listen_file;
			_files[LOCAL_NODE]   = &_local_file;
			_files[REMOTE_NODE]  = &_remote_file;
		}

		~Lxip_socket_dir()
		{
			_accept_file.dissolve_handles();
			_bind_file.dissolve_handles();
			_connect_file.dissolve_handles();
			_data_file.dissolve_handles();
			_peek_file.dissolve_handles();
			_listen_file.dissolve_handles();
			_local_file.dissolve_handles();
			_remote_file.dissolve_handles();


			genode_socket_release(&_sock);
			_parent.release(id);
		}

		/**************************
		 ** Socket_dir interface **
		 **************************/

		Lxip::Protocol_dir &parent() override { return _parent; }

		genode_sockaddr &remote_addr() override { return _remote_addr; }

		char const *top_dir() override { return _parent.top_dir(); }

		Open_result
		open(Vfs::File_system &fs,
		     Genode::Allocator &alloc,
		     char const *path, unsigned mode, Vfs::Vfs_handle**out_handle) override
		{
			++path;

			if (strcmp(path, "accept_socket") == 0)
				return _accept_new_socket(fs, alloc, out_handle);

			for (Vfs::File *f : _files) {
				if (f && Genode::strcmp(f->name(), path) == 0) {
					Vfs::Lxip_vfs_file_handle *handle = new (alloc)
						Vfs::Lxip_vfs_file_handle(fs, alloc, mode, f);
					*out_handle = handle;
					return Open_result::OPEN_OK;
				}
			}

			Genode::error(path, " is UNACCESSIBLE");
			return Vfs::Directory_service::OPEN_ERR_UNACCESSIBLE;
		}

		void bind(bool) override { }

		long bind() override { return _bind_file.port(); }

		bool lookup_port(long port) override { return _parent.lookup_port(port); }

		void connect(bool) override { }

		void listen(bool v) override
		{
			_files[ACCEPT_SOCKET_NODE] = v ? &_accept_socket_file : nullptr;
		}

		bool _closed = false;

		void close()        override { _closed = true; }
		bool closed() const override { return _closed; }


		/*************************
		 ** Directory interface **
		 *************************/

		Vfs::Node *child(char const *name) override
		{
			for (Vfs::File *n : _files)
				if (n && Genode::strcmp(n->name(), name) == 0)
					return n;

			return nullptr;
		}

		file_size num_dirent() override { return _num_nodes(); }

		long read(Byte_range_ptr const &dst,
		                   file_size seek_offset) override
		{
			typedef Vfs::Directory_service::Dirent Dirent;

			if (dst.num_bytes < sizeof(Dirent))
				return -1;

			size_t index = size_t(seek_offset / sizeof(Dirent));

			Dirent &out = *(Dirent*)dst.start;

			Vfs::Node *node = nullptr;
			for (Vfs::File *n : _files) {
				if (n) {
					if (index == 0) {
						node = n;
						break;
					}
					--index;
				}
			}
			if (!node) {
				out = {
					.fileno = index + 1,
					.type   = Directory_service::Dirent_type::END,
					.rwx    = { },
					.name   = { } };

				return -1;
			}

			out = {
				.fileno = index + 1,
				.type   = Directory_service::Dirent_type::TRANSACTIONAL_FILE,
				.rwx    = Node_rwx::rw(),
				.name   = { node->name() } };

			return sizeof(Dirent);
		}

		Lxip_socket_dir(const Lxip_socket_dir&) = delete;
		Lxip_socket_dir operator=(const Lxip_socket_dir&) = delete;
};


struct Vfs::Lxip_socket_handle final : Vfs::Lxip_vfs_handle
{
		Lxip_socket_dir socket_dir;

		Lxip_socket_handle(Vfs::File_system &fs,
		                   Genode::Allocator &alloc,
		                   Lxip::Protocol_dir &parent,
		                   genode_socket_handle &sock)
		:
			Lxip_vfs_handle(fs, alloc, 0),
			socket_dir(alloc, parent, sock)
		{ }

		bool read_ready() const override { return true; }

		Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
		{
			out_count = Format::snprintf(
				dst.start, dst.num_bytes, "%s/%s\n", socket_dir.parent().name(), socket_dir.name());
			return Read_result::READ_OK;
		}

		Write_result write(Const_byte_range_ptr const &, size_t &) override {
			return Write_result::WRITE_ERR_INVALID; }
};


Vfs::Directory_service::Open_result
Vfs::Lxip_socket_dir::_accept_new_socket(Vfs::File_system &fs,
                                         Genode::Allocator &alloc,
                                         Vfs::Vfs_handle **out_handle)
{
	Open_result res = Open_result::OPEN_ERR_UNACCESSIBLE;
	if (!_files[ACCEPT_SOCKET_NODE]) return res;

	Errno err;
	genode_socket_handle *new_sock = genode_socket_accept(&_sock, nullptr, &err);
	if (err != GENODE_ENONE) {
		error("accept socket failed");
		return res;
	}

	try {
		Vfs::Lxip_socket_handle *handle = new (alloc)
			Vfs::Lxip_socket_handle(fs, alloc, _parent, *new_sock);
		*out_handle = handle;
		return Vfs::Directory_service::Open_result::OPEN_OK;
	}

	catch (Genode::Out_of_ram)  { res = Open_result::OPEN_ERR_OUT_OF_RAM;  }
	catch (Genode::Out_of_caps) { res = Open_result::OPEN_ERR_OUT_OF_CAPS; }
	catch (...) { Genode::error("unhandle error during accept"); }

	genode_socket_release(new_sock);
	return res;
};


class Lxip::Protocol_dir_impl : public Protocol_dir
{
	private:

		Genode::Allocator        &_alloc;
		Vfs::File_system         &_parent;

		struct New_socket_file : Vfs::File
		{
			New_socket_file() : Vfs::File("new_socket") { }
		} _new_socket_file { };

		Type const _type;

		/**************************
		 ** Simple node registry **
		 **************************/

		enum { MAX_NODES = Lxip::MAX_SOCKETS + 1 };
		Vfs::Node *_nodes[MAX_NODES];

		unsigned _num_nodes()
		{
			unsigned n = 0;
			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				n += (_nodes[i] != nullptr);
			return n;
		}

		Vfs::Node **_unused_node()
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				if (_nodes[i] == nullptr) return &_nodes[i];
			throw -1;
		}

		void _free_node(Vfs::Node *node)
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				if (_nodes[i] == node) {
					_nodes[i] = nullptr;
					break;
				}
		}

		bool _is_root(const char *path)
		{
			return (Genode::strcmp(path, "") == 0) || (Genode::strcmp(path, "/") == 0);
		}

		Vfs::Directory_service::Open_result
		_open_new_socket(Vfs::File_system &fs,
		                 Genode::Allocator &alloc,
		                 Vfs::Vfs_handle **out_handle)
		{
			Vfs::Directory_service::Open_result res =
				Vfs::Directory_service::Open_result::OPEN_ERR_UNACCESSIBLE;

			int type = (_type == Lxip::Protocol_dir::TYPE_STREAM)
			         ? SOCK_STREAM : SOCK_DGRAM;

			Errno err;
			genode_socket_handle *sock = genode_socket(AF_INET, type, 0, &err);
			if (sock == nullptr) return res;

			/* XXX always allow UDP broadcast */
			if (type == SOCK_DGRAM) {
				int enable = 1;
				genode_socket_setsockopt(sock, GENODE_SOL_SOCKET, GENODE_SO_BROADCAST,
				                         &enable, sizeof(enable));
			}

			try {
				Vfs::Lxip_socket_handle *handle = new (alloc)
					Vfs::Lxip_socket_handle(fs, alloc, *this, *sock);
				*out_handle = handle;
				return Vfs::Directory_service::Open_result::OPEN_OK;
			}

			catch (Genode::Out_of_ram)  {
				res = Open_result::OPEN_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) {
				res = Open_result::OPEN_ERR_OUT_OF_CAPS; }
			catch (...) { Genode::error("unhandle error during accept"); }

			genode_socket_release(sock);
			return res;
		}

	public:

		Protocol_dir_impl(Genode::Allocator        &alloc,
		                  Vfs::File_system         &parent,
		                  char               const *name,
		                  Lxip::Protocol_dir::Type  type)
		:
			Protocol_dir(name),
			_alloc(alloc), _parent(parent), _type(type)
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++) {
				_nodes[i] = nullptr;
			}

			_nodes[0] = &_new_socket_file;
		}

		~Protocol_dir_impl() { }

		Vfs::Node *lookup(char const *path)
		{
			if (*path == '/') path++;
			if (*path == '\0') return this;

			char const *p = path;
			while (*++p && *p != '/');

			for (Genode::size_t i = 0; i < MAX_NODES; i++) {
				if (!_nodes[i]) continue;

				if (Genode::strcmp(_nodes[i]->name(), path, (p - path)) == 0) {
					Vfs::Directory *dir = dynamic_cast<Directory *>(_nodes[i]);
					if (!dir) return _nodes[i];

					Socket_dir *socket = dynamic_cast<Socket_dir *>(_nodes[i]);
					if (socket && socket->closed())
						return nullptr;

					if (*p == '/') return dir->child(p+1);
					else           return dir;
				}
			}

			return nullptr;
		}

		Vfs::Directory_service::Unlink_result unlink(char const *path)
		{
			Vfs::Node *node = lookup(path);
			if (!node) return Vfs::Directory_service::UNLINK_ERR_NO_ENTRY;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (!dir) return Vfs::Directory_service::UNLINK_ERR_NO_ENTRY;

			_free_node(node);

			Genode::destroy(&_alloc, dir);

			return Vfs::Directory_service::UNLINK_OK;
		}

		/****************************
		 ** Protocol_dir interface **
		 ****************************/

		char const *top_dir() override { return name(); }

		Type type() override { return _type; }

		Open_result open(Vfs::File_system &fs,
		                 Genode::Allocator &alloc,
		                 char const *path, unsigned mode,
		                 Vfs::Vfs_handle **out_handle) override
		{
			if (Genode::strcmp(path, "/new_socket") == 0) {
				if (mode != 0) return Open_result::OPEN_ERR_NO_PERM;
				return _open_new_socket(fs, alloc, out_handle);
			}

			path++;
			char const *p = path;
			while (*++p && *p != '/');

			for (Genode::size_t i = 1; i < MAX_NODES; i++) {
				if (!_nodes[i]) continue;
				if (Genode::strcmp(_nodes[i]->name(), path, (p - path)) == 0) {
					Vfs::Directory *dir = dynamic_cast<Directory *>(_nodes[i]);
					if (dir) {
						path += (p - path);
						return dir->open(fs, alloc, path, mode, out_handle);
					}
				}
			}

			return Open_result::OPEN_ERR_UNACCESSIBLE;
		}

		unsigned adopt_socket(Lxip::Socket_dir &dir) override
		{
			Vfs::Node **node = _unused_node();
			if (!node) throw -1;

			unsigned long const id = ((unsigned char*)node - (unsigned char*)_nodes)/sizeof(*_nodes);

			*node = &dir;
			return unsigned(id);
		}

		void release(unsigned id) override
		{
			if (id < MAX_NODES)
				_nodes[id] = nullptr;
		}

		bool lookup_port(long port) override
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++) {
				if (_nodes[i] == nullptr) continue;

				Lxip::Socket_dir *dir = dynamic_cast<Lxip::Socket_dir*>(_nodes[i]);
				if (dir && dir->bind() == port) return true;
			}
			return false;
		}

		/*************************
		 ** Directory interface **
		 *************************/

		Vfs::file_size num_dirent() override { return _num_nodes(); }

		long read(Vfs::Byte_range_ptr const &dst,
		                   Vfs::file_size seek_offset) override
		{
			typedef Vfs::Directory_service::Dirent Dirent;

			if (dst.num_bytes < sizeof(Dirent))
				return -1;

			size_t index = size_t(seek_offset / sizeof(Dirent));

			Dirent &out = *(Dirent*)dst.start;

			Vfs::Node *node = nullptr;
			for (Vfs::Node *n : _nodes) {
				if (n) {
					if (index == 0) {
						node = n;
						break;
					}
					--index;
				}
			}
			if (!node) {
				out = {
					.fileno = index + 1,
					.type   = Vfs::Directory_service::Dirent_type::END,
					.rwx    = { },
					.name   = { } };

				return -1;
			}

			typedef Vfs::Directory_service::Dirent_type Dirent_type;

			Dirent_type const type =
				dynamic_cast<Vfs::Directory*>(node) ? Dirent_type::DIRECTORY :
				dynamic_cast<Vfs::File     *>(node) ? Dirent_type::TRANSACTIONAL_FILE
				                                    : Dirent_type::END;

			Vfs::Node_rwx const rwx = (type == Dirent_type::DIRECTORY)
			                        ? Vfs::Node_rwx::rwx()
			                        : Vfs::Node_rwx::rw();

			out = {
				.fileno = index + 1,
				.type   = type,
				.rwx    = rwx,
				.name   = { node->name() } };

			return sizeof(Dirent);
		}

		Vfs::Node *child(char const *) override { return nullptr; }

		Protocol_dir_impl(const Protocol_dir_impl&) = delete;
		Protocol_dir_impl operator=(const Protocol_dir_impl&) = delete;
};


class Vfs::Lxip_address_file final : public Vfs::File
{
	private:

		unsigned int &_numeric_address;

	public:

		Lxip_address_file(char const *name, unsigned int &numeric_address)
		: Vfs::File(name), _numeric_address(numeric_address) { }

		long read(Lxip_vfs_file_handle &,
		                   Byte_range_ptr const &dst,
		                   file_size /* ignored */) override
		{
			enum {
				MAX_ADDRESS_STRING_SIZE = sizeof("000.000.000.000\n")
			};

			Genode::String<MAX_ADDRESS_STRING_SIZE> address {
				Net::Ipv4_address(&_numeric_address)
			};

			size_t n = min(dst.num_bytes, strlen(address.string()));
			memcpy(dst.start, address.string(), n);
			if (n < dst.num_bytes)
				dst.start[n++] = '\n';

			return n;
		}
};


class Vfs::Lxip_link_state_file final : public Vfs::File
{
	private:

		bool &_numeric_link_state;

	public:

		Lxip_link_state_file(char const *name, bool &numeric_link_state)
		: Vfs::File(name), _numeric_link_state(numeric_link_state) { }

		long read(Lxip_vfs_file_handle &,
		          Byte_range_ptr const &dst,
		          file_size /* ignored */) override
		{
			enum {
				MAX_LINK_STATE_STRING_SIZE = sizeof("down\n")
			};

			Genode::String<MAX_LINK_STATE_STRING_SIZE> link_state {
				_numeric_link_state ? "up" : "down"
			};

			size_t n = min(dst.num_bytes, strlen(link_state.string()));
			memcpy(dst.start, link_state.string(), n);
			if (n < dst.num_bytes)
				dst.start[n++] = '\n';

			return n;
		}
};


extern "C" unsigned int ic_myaddr;
extern "C" unsigned int ic_netmask;
extern "C" unsigned int ic_gateway;
extern "C" unsigned int ic_nameservers[1];

//XXX: handle in lxip
bool ic_link_state = true;


/*******************************
 ** Filesystem implementation **
 *******************************/

class Vfs::Lxip_file_system : public Vfs::File_system,
                              public Vfs::Directory
{
	private:

		Genode::Entrypoint       &_ep;
		Genode::Allocator        &_alloc;

		Lxip::Protocol_dir_impl _tcp_dir {
			_alloc, *this, "tcp", Lxip::Protocol_dir::TYPE_STREAM };
		Lxip::Protocol_dir_impl _udp_dir {
			_alloc, *this, "udp", Lxip::Protocol_dir::TYPE_DGRAM  };

		Lxip_address_file    _address    { "address",    ic_myaddr };
		Lxip_address_file    _netmask    { "netmask",    ic_netmask };
		Lxip_address_file    _gateway    { "gateway",    ic_gateway };
		Lxip_address_file    _nameserver { "nameserver", ic_nameservers[0] };
		Lxip_link_state_file _link_state { "link_state", ic_link_state };

		Vfs::Node *_lookup(char const *path)
		{
			if (*path == '/') path++;
			if (*path == '\0') return this;

			if (Genode::strcmp(path, "tcp", 3) == 0)
				return _tcp_dir.lookup(&path[3]);

			if (Genode::strcmp(path, "udp", 3) == 0)
				return _udp_dir.lookup(&path[3]);

			if (Genode::strcmp(path, _address.name(),
			                   strlen(_address.name()) + 1) == 0)
				return &_address;

			if (Genode::strcmp(path, _netmask.name(),
			                   strlen(_netmask.name()) + 1) == 0)
				return &_netmask;

			if (Genode::strcmp(path, _gateway.name(),
			                   strlen(_gateway.name()) + 1) == 0)
				return &_gateway;

			if (Genode::strcmp(path, _nameserver.name(),
			                   strlen(_nameserver.name()) + 1) == 0)
				return &_nameserver;

			if (Genode::strcmp(path, _link_state.name(),
			                   strlen(_link_state.name()) + 1) == 0)
				return &_link_state;

			return nullptr;
		}

		bool _is_root(const char *path)
		{
			return (strcmp(path, "") == 0) || (strcmp(path, "/") == 0);
		}

		Read_result _read(Vfs::Vfs_handle *vfs_handle, Byte_range_ptr const &dst,
		                  size_t &out_count)
		{
			Vfs::Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);

			return handle->read(dst, out_count);
		}

	public:

		Lxip_file_system(Vfs::Env &env, Genode::Xml_node config)
		:
			Directory(""),
			_ep(env.env().ep()), _alloc(env.alloc())
		{
			apply_config(config);
		}

		~Lxip_file_system() { }

		char const *name()          { return "lxip"; }
		char const *type() override { return "lxip"; }

		/***************************
		 ** File_system interface **
		 ***************************/

		void apply_config(Genode::Xml_node const &config) override
		{
			typedef String<16> Addr;

			unsigned const mtu = config.attribute_value("mtu", 0U);
			if (mtu) {
				log("Setting MTU to ", mtu);
				genode_socket_configure_mtu(mtu);
			} else {
				genode_socket_configure_mtu(0);
			}

			if (config.attribute_value("dhcp", false)) {
				log("Using DHCP for interface configuration.");
				genode_socket_config address_config = { .dhcp = true };
				genode_socket_config_address(&address_config);
				return;
			}

			try {

				Addr ip_addr    = config.attribute_value("ip_addr", Addr());
				Addr netmask    = config.attribute_value("netmask", Addr());
				Addr gateway    = config.attribute_value("gateway", Addr());
				Addr nameserver = config.attribute_value("nameserver", Addr());

				if (ip_addr == "") {
					warning("Missing \"ip_addr\" attribute. Ignoring network interface config.");
					throw Genode::Xml_node::Nonexistent_attribute();
				} else if (netmask == "") {
					warning("Missing \"netmask\" attribute. Ignoring network interface config.");
					throw Genode::Xml_node::Nonexistent_attribute();
				}

				log("static network interface: ip_addr=",ip_addr," netmask=",netmask);

				genode_socket_config address_config = {
					.dhcp       = false,
					.ip_addr    = ip_addr.string(),
					.netmask    = netmask.string(),
					.gateway    = gateway.string(),
					.nameserver = nameserver.string(),
				};

				genode_socket_config_address(&address_config);
			} catch (...) { }
		 }


		/*************************
		 ** Directory interface **
		 *************************/

		file_size num_dirent() override { return 7; }

		Vfs::Directory::Open_result
		open(Vfs::File_system &,
	         Genode::Allocator &,
	         char const*, unsigned, Vfs::Vfs_handle**) override {
			return Vfs::Directory::Open_result::OPEN_ERR_UNACCESSIBLE; }

		long read(Byte_range_ptr const &dst, file_size seek_offset) override
		{
			if (dst.num_bytes < sizeof(Dirent))
				return -1;

			file_size const index = seek_offset / sizeof(Dirent);

			struct Entry
			{
				void const *fileno;
				Dirent_type type;
				char const *name;
			};

			enum { NUM_ENTRIES = 8U };
			static Entry const entries[NUM_ENTRIES] = {
				{ &_tcp_dir,    Dirent_type::DIRECTORY,          "tcp" },
				{ &_udp_dir,    Dirent_type::DIRECTORY,          "udp" },
				{ &_address,    Dirent_type::TRANSACTIONAL_FILE, "address" },
				{ &_netmask,    Dirent_type::TRANSACTIONAL_FILE, "netmask" },
				{ &_gateway,    Dirent_type::TRANSACTIONAL_FILE, "gateway" },
				{ &_nameserver, Dirent_type::TRANSACTIONAL_FILE, "nameserver" },
				{ &_link_state, Dirent_type::TRANSACTIONAL_FILE, "link_state" },
				{ nullptr,      Dirent_type::END,                "" }
			};

			Entry const &entry = entries[min(index, NUM_ENTRIES - 1U)];

			Dirent &out = *(Dirent*)dst.start;

			out = {
				.fileno = (Genode::addr_t)entry.fileno,
				.type   = entry.type,
				.rwx    = entry.type == Dirent_type::DIRECTORY
				        ? Node_rwx::rwx() : Node_rwx::rw(),
				.name   = { entry.name }
			};
			return sizeof(Dirent);
		}

		Vfs::Node *child(char const *) override { return nullptr; }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *) override {
			return Dataspace_capability(); }

		void release(char const *, Dataspace_capability) override { }

		Stat_result stat(char const *path, Stat &out) override
		{
			Node *node = _lookup(path);
			if (!node) return STAT_ERR_NO_ENTRY;

			out = { };

			if (dynamic_cast<Vfs::Directory*>(node)) {
				out.type = Node_type::DIRECTORY;
				out.rwx  = Node_rwx::rwx();
				out.size = 1;
				return STAT_OK;
			}

			if (dynamic_cast<Lxip_data_file*>(node)) {
				out.type = Node_type::CONTINUOUS_FILE;
				out.rwx  = Node_rwx::rw();
				out.size = 0;
				return STAT_OK;
			}

			if (dynamic_cast<Lxip_peek_file*>(node)) {
				out.type = Node_type::CONTINUOUS_FILE;
				out.rwx  = Node_rwx::rw();
				out.size = 0;
				return STAT_OK;
			}

			if (dynamic_cast<Vfs::File*>(node)) {
				out.type = Node_type::TRANSACTIONAL_FILE;
				out.rwx  = Node_rwx::rw();
				out.size = 0x1000;  /* there may be something to read */
				return STAT_OK;
			}

			return STAT_ERR_NO_ENTRY;
		}

		file_size num_dirent(char const *path) override
		{
			if (_is_root(path)) return num_dirent();

			Vfs::Node *node = _lookup(path);
			if (!node) return 0;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (!dir) return 0;

			return dir->num_dirent();
		}

		bool directory(char const *path) override
		{
			Vfs::Node *node = _lookup(path);
			return node ? dynamic_cast<Vfs::Directory *>(node) : 0;
		}

		char const *leaf_path(char const *path) override
		{
			Vfs::Node *node = _lookup(path);
			return node ? path : nullptr;
		}

		Vfs::Directory_service::Open_result
		open(char const *path, unsigned mode,
		     Vfs_handle **out_handle,
		     Genode::Allocator &alloc) override
		{
			if (mode & OPEN_MODE_CREATE) return OPEN_ERR_NO_PERM;

			try {
				if (Genode::strcmp(path, "/tcp", 4) == 0)
					return _tcp_dir.open(*this, alloc,
					                     &path[4], mode, out_handle);
				if (Genode::strcmp(path, "/udp", 4) == 0)
					return _udp_dir.open(*this, alloc,
					                     &path[4], mode, out_handle);

				Vfs::Node *node = _lookup(path);
				if (!node) return OPEN_ERR_UNACCESSIBLE;

				Vfs::File *file = dynamic_cast<Vfs::File*>(node);
				if (file) {
					Lxip_vfs_file_handle *handle =
						new (alloc) Vfs::Lxip_vfs_file_handle(*this, alloc, 0, file);
					*out_handle = handle;
					return OPEN_OK;
				}
			}
			catch (Genode::Out_of_ram ) { return OPEN_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }

			return OPEN_ERR_UNACCESSIBLE;
		}

		Opendir_result opendir(char const *path, bool /* create */,
		                       Vfs_handle **out_handle, Allocator &alloc) override
		{
			Vfs::Node *node = _lookup(path);

			if (!node) return OPENDIR_ERR_LOOKUP_FAILED;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (dir) {
				Lxip_vfs_dir_handle *handle =
					new (alloc) Vfs::Lxip_vfs_dir_handle(*this, alloc, 0, *dir);
				*out_handle = handle;

				return OPENDIR_OK;
			}

			return OPENDIR_ERR_LOOKUP_FAILED;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);

			Lxip_vfs_file_handle *file_handle =
				dynamic_cast<Vfs::Lxip_vfs_file_handle*>(handle);

			if (file_handle)
				_read_ready_waiters_ptr->remove(file_handle->read_ready_elem);

			Genode::destroy(handle->alloc(), handle);
		}

		Unlink_result unlink(char const *path) override
		{
			if (*path == '/') path++;

			if (Genode::strcmp(path, "tcp", 3) == 0)
				return _tcp_dir.unlink(&path[3]);
			if (Genode::strcmp(path, "udp", 3) == 0)
				return _udp_dir.unlink(&path[3]);
			return UNLINK_ERR_NO_ENTRY;
		}

		Rename_result rename(char const *, char const *) override {
			return RENAME_ERR_NO_PERM; }

		/*************************************
		 ** Lxip_file I/O service interface **
		 *************************************/

		Write_result write(Vfs_handle *vfs_handle,
		                   Vfs::Const_byte_range_ptr const &src,
		                   size_t &out_count) override
		{
			Vfs::Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);

			try { return handle->write(src, out_count); }
			catch (File::Would_block) { return WRITE_ERR_WOULD_BLOCK; }

		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          Vfs::Byte_range_ptr const &dst,
		                          size_t &out_count) override
		{
			try { return _read(vfs_handle, dst, out_count); }
			catch (File::Would_block) { return READ_QUEUED; }
		}

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			/* report ok because libc always executes ftruncate() when opening rw */
			return FTRUNCATE_OK;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			Lxip_vfs_file_handle *handle =
				dynamic_cast<Vfs::Lxip_vfs_file_handle *>(vfs_handle);

			if (handle) {
				if (!handle->read_ready_elem.enqueued())
					_read_ready_waiters_ptr->enqueue(handle->read_ready_elem);
				return true;
			}

			return false;
		}

		bool read_ready(Vfs_handle const &vfs_handle) const override
		{
			Lxip_vfs_handle const &handle =
				static_cast<Lxip_vfs_handle const &>(vfs_handle);

			return handle.read_ready();
		}

		bool write_ready(Vfs_handle const &) const override
		{
			/* wakeup from WRITE_ERR_WOULD_BLOCK not supported */
			return true;
		}

		Sync_result complete_sync(Vfs_handle *vfs_handle) override
		{
			Vfs::Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);
			return handle->sync();
		}
};


struct Lxip_factory : Vfs::File_system_factory
{

	/* wakup user task */
	static void socket_progress(void *data)
	{
		Vfs::Env *env = static_cast<Vfs::Env *>(data);
		env->user().wakeup_vfs_user();
		poll_all();
	}

	struct genode_socket_io_progress io_progress { };

	Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
	{
		io_progress.data = &env;
		io_progress.callback = socket_progress;

		genode_socket_init(genode_env_ptr(env.env()), &io_progress);

		return new (env.alloc()) Vfs::Lxip_file_system(env, config);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Vfs::Lxip_vfs_file_handle::Fifo read_ready_waiters;

	_read_ready_waiters_ptr = &read_ready_waiters;

	static Lxip_factory factory;
	return &factory;
}

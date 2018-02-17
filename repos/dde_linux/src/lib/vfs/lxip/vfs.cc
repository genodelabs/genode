/*
 * \brief  lxip-based socket file system
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/snprintf.h>
#include <net/ipv4.h>
#include <util/string.h>
#include <util/xml_node.h>
#include <vfs/directory_service.h>
#include <vfs/file_io_service.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>

/* Lxip includes */
#include <lxip/lxip.h>
#include <lx.h>

/* Lx_kit */
#include <lx_kit/env.h>
#include <lx_kit/malloc.h>


namespace Linux {
	#include <lx_emul.h>
	#include <msghdr.h>

	#include <lx_emul/extern_c_begin.h>
	#include <linux/socket.h>
	#include <uapi/linux/in.h>
	#include <uapi/linux/if.h>
	extern int sock_setsockopt(socket *sock, int level,
	                           int op, char __user *optval,
	                           unsigned int optlen);
	extern int sock_getsockopt(socket *sock, int level,
	                           int op, char __user *optval,
	                           int __user *optlen);
	socket *sock_alloc(void);
	#include <lx_emul/extern_c_end.h>

	enum {
		POLLIN_SET  = (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR),
		POLLOUT_SET = (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR),
		POLLEX_SET  = (POLLPRI)
	};
}


namespace {

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

		unsigned long result = 0;
		p += Genode::ascii_to_unsigned(p, result, 10);

		c = result;

		if (*p == '.') ++p;
		if (*p == 0) break;
	};

	return (to[0]<<0)|(to[1]<<8)|(to[2]<<16)|(to[3]<<24);
}

}


namespace Lxip {

	using namespace Linux;

	struct Protocol_dir;
	struct Socket_dir;

	class Protocol_dir_impl;

	enum {
		MAX_SOCKETS         = 128,       /* 3 */
		MAX_SOCKET_NAME_LEN = 3 + 1,     /* + \0 */
		MAX_DATA_LEN        = 32,        /* 255.255.255.255:65536 + something */
	};
}


struct Lxip::Protocol_dir
{
	enum Type { TYPE_STREAM, TYPE_DGRAM };

	virtual char const *top_dir() = 0;
	virtual Type type() = 0;
	virtual unsigned adopt_socket(socket &, bool) = 0;
	virtual bool     lookup_port(long) = 0;
};


struct Lxip::Socket_dir
{
	virtual Protocol_dir &parent() = 0;
	virtual char const *top_dir() = 0;
	virtual unsigned accept(socket&) = 0;
	virtual void     bind(bool) = 0; /* bind to port */
	virtual long     bind() = 0; /* return bound port */
	virtual bool     lookup_port(long) = 0;
	virtual void     connect(bool) = 0;
	virtual void     listen(bool) = 0;
	virtual sockaddr_storage &remote_addr() = 0;
	virtual void     close() = 0;
	virtual bool     closed() const = 0;
	virtual void trigger_io_response(Vfs::Vfs_handle::Context *) = 0;
};


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
	class Lxip_socket_dir;

	class Lxip_new_socket_file;

	class Lxip_link_state_file;
	class Lxip_address_file;

	class Lxip_vfs_handle;
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

	virtual char const *name() { return _name; }

	virtual void close() { }

	/**
	 * Pass len data to handle read callback
	 */
	virtual Lxip::ssize_t read(char *, Genode::size_t, file_size)
	{
		Genode::error("lxip: read from write-only handle");
		return -1;
	}

	/**
	 * Pass len data to handle write callback
	 */
	virtual Lxip::ssize_t write(char const *, Genode::size_t, file_size)
	{
		Genode::error("lxip: write to read-only handle");
		return -1;
	}
};


struct Vfs::File : Vfs::Node
{
	Lxip_vfs_file_handles handles;

	File(char const *name) : Node(name) { }

	virtual ~File() { }

	/**
	 * Read or write operation would block exception
	 */
	struct Would_block { };

	/**
	 * Check for data to read or write
	 */
	virtual bool poll(bool trigger_io_response, Vfs::Vfs_handle::Context *context) = 0;
};


struct Vfs::Directory : Vfs::Node
{
	Directory(char const *name) : Node(name) { }

	virtual ~Directory() { };

	virtual Vfs::Node *child(char const *)                        = 0;
	virtual file_size num_dirent()                                = 0;
};


struct Vfs::Lxip_vfs_handle : Vfs::Vfs_handle
{
	Node &node;

	Lxip_vfs_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                Vfs::Node &node)
	: Vfs::Vfs_handle(fs, fs, alloc, status_flags),	node(node) { }
};


struct Vfs::Lxip_vfs_file_handle final : Vfs::Lxip_vfs_handle
{
	Vfs::File &file;

	List_element<Lxip_vfs_file_handle> file_le    { this };
	List_element<Lxip_vfs_file_handle> polling_le { this };

	Lxip_vfs_file_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                     Vfs::File &file)
	: Lxip_vfs_handle(fs, alloc, status_flags, file), file(file)
	{
		file.handles.insert(&file_le);
	}

	~Lxip_vfs_file_handle()
	{
		file.handles.remove(&file_le);
	}
};


struct Vfs::Lxip_vfs_dir_handle final : Vfs::Lxip_vfs_handle
{
	Vfs::Directory &dir;

	Lxip_vfs_dir_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                    Vfs::Directory &dir)
	: Vfs::Lxip_vfs_handle(fs, alloc, status_flags, dir),
	  dir(dir) { }
};


/**
 * List of open handles to potentially poll
 *
 * Could be a dynamic queue, but this works for now.
 */
static Vfs::Lxip_vfs_file_handles _polling_handles;

static void poll_all()
{
	using namespace Linux;

	for (Genode::List_element<Vfs::Lxip_vfs_file_handle> *le = _polling_handles.first();
	     le; le = le->next())
	{
		Vfs::Lxip_vfs_file_handle *handle = le->object();
		handle->file.poll(true, handle->context);
	}
}


/*****************************
 ** Lxip vfs specific nodes **
 *****************************/

class Vfs::Lxip_file : public Vfs::File
{
	protected:

		Lxip::Socket_dir &_parent;
		Linux::socket    &_sock;

		char _content_buffer[Lxip::MAX_DATA_LEN];

		bool _write_content_line(char const *buf, Genode::size_t len)
		{
			if (len > sizeof(_content_buffer) - 2)
				return false;

			Genode::memcpy(_content_buffer, buf, len);
			_content_buffer[len+0] = '\n';
			_content_buffer[len+1] = '\0';
			return true;
		}

		bool _sock_valid() { return _sock.sk != nullptr; }

	public:

		Lxip_file(Lxip::Socket_dir &p, Linux::socket &s, char const *name)
		: Vfs::File(name), _parent(p), _sock(s) { }

		virtual ~Lxip_file() { }

		void dissolve_handles()
		{
			for (Genode::List_element<Vfs::Lxip_vfs_file_handle> *le = handles.first();
			     le; le = le->next())
			{
				_polling_handles.remove(&le->object()->polling_le);
			}
		}
};


class Vfs::Lxip_data_file : public Vfs::Lxip_file
{
	public:

		Lxip_data_file(Lxip::Socket_dir &p, Linux::socket &s)
		: Lxip_file(p, s, "data") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll(bool trigger_io_response,
		          Vfs::Vfs_handle::Context *context) override
		{
			using namespace Linux;

			file f;
			f.f_flags = 0;
			if (_sock.ops->poll(&f, &_sock, nullptr) & (POLLIN_SET)) {
				if (trigger_io_response)
					_parent.trigger_io_response(context);
				return true;
			}
			return false;
		}

		Lxip::ssize_t write(char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			iovec iov { const_cast<char *>(src), len };

			msghdr msg = create_msghdr(&_parent.remote_addr(),
			                           sizeof(sockaddr_in), len, &iov);

			return _sock.ops->sendmsg(&_sock, &msg, len);
		}

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			iovec iov { dst, len };

			msghdr msg = create_msghdr(nullptr, 0, len, &iov);

			Lxip::ssize_t ret = _sock.ops->recvmsg(&_sock, &msg, len, MSG_DONTWAIT);
			if (ret == -EAGAIN)
				throw Would_block();
			return ret;
		}
};


class Vfs::Lxip_bind_file : public Vfs::Lxip_file
{
	private:

		long _port = -1;

	public:

		Lxip_bind_file(Lxip::Socket_dir &p, Linux::socket &s)
		: Lxip_file(p, s, "bind") { }

		long port() { return _port; }

		/********************
		 ** File interface **
		 ********************/

		bool poll(bool, Vfs::Vfs_handle::Context *) { return true; }

		Lxip::ssize_t write(char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			/* already bound to port */
			if (_port >= 0) return -1;

			if (!_write_content_line(src, len)) return -1;

			/* check if port is already used by other socket */
			long port = get_port(_content_buffer);
			if (port == -1)                return -1;
			if (_parent.lookup_port(port)) return -1;

			/* port is free, try to bind it */
			sockaddr_storage addr_storage;

			sockaddr_in *addr     = (sockaddr_in *)&addr_storage;
			addr->sin_port        = htons(port);
			addr->sin_addr.s_addr = get_addr(_content_buffer);
			addr->sin_family      = AF_INET;

			int res = _sock.ops->bind(&_sock, (sockaddr*)addr, sizeof(addr_storage));
			if (res != 0) return -1;

			_port = port;

			_parent.bind(true);

			return len;
		}

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			if (len < sizeof(_content_buffer))
				return -1;

			Genode::size_t const n = Genode::strlen(_content_buffer);
			Genode::memcpy(dst, _content_buffer, n);

			return n;
		}
};


class Vfs::Lxip_listen_file : public Vfs::Lxip_file
{
	public:

		Lxip_listen_file(Lxip::Socket_dir &p, Linux::socket &s)
		: Lxip_file(p, s, "listen") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll(bool, Vfs::Vfs_handle::Context *) { return true; }

		Lxip::ssize_t write(char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			if (!_sock_valid()) return -1;

			if (!_write_content_line(src, len)) return -1;

			Lxip::ssize_t res;

			unsigned long backlog = 5; /* default */
			Genode::ascii_to_unsigned(src, backlog, 10);

			res = _sock.ops->listen(&_sock, backlog);
			if (res != 0) {
				_write_content_line("", 0);
				return -1;
			}

			_parent.listen(true);

			return len;
		}

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			if (len < sizeof(_content_buffer))
				return -1;

			Genode::size_t const n = Genode::strlen(_content_buffer);
			Genode::memcpy(dst, _content_buffer, n);

			return n;
		}
};


class Vfs::Lxip_connect_file : public Vfs::Lxip_file
{
	private:

		bool _connecting   = false;
		bool _is_connected = false;

	public:

		Lxip_connect_file(Lxip::Socket_dir &p, Linux::socket &s)
		: Lxip_file(p, s, "connect") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll(bool, Vfs::Vfs_handle::Context *) { return true; }

		Lxip::ssize_t write(char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			if (!_write_content_line(src, len)) return -1;

			long const port = get_port(_content_buffer);
			if (port == -1) return -1;

			sockaddr_storage addr_storage;

			sockaddr_in *addr     = (sockaddr_in *)&addr_storage;
			addr->sin_port        = htons(port);
			addr->sin_addr.s_addr = get_addr(_content_buffer);
			addr->sin_family      = AF_INET;

			int res = _sock.ops->connect(&_sock, (sockaddr *)addr, sizeof(addr_storage), 0);

			switch (res) {
			case Lxip::Io_result::LINUX_EINPROGRESS:
				_connecting = true;
				return -1;

			case Lxip::Io_result::LINUX_EALREADY:
				return -1;

			case Lxip::Io_result::LINUX_EISCONN:
				/*
				 * Connecting on an already connected socket is an error.
				 * If we get this error after we got EINPROGRESS it is
				 * fine.
				 */
				if (_is_connected || !_connecting) return -1;
				_is_connected = true;
				break;

			default:
				if (res != 0) return -1;
				break;
			}

			sockaddr_in *remote_addr     = (sockaddr_in *)&_parent.remote_addr();
			remote_addr->sin_port        = htons(port);
			remote_addr->sin_addr.s_addr = get_addr(_content_buffer);
			remote_addr->sin_family      = AF_INET;

			_parent.connect(true);

			return len;
		}
};


class Vfs::Lxip_local_file : public Vfs::Lxip_file
{
	public:

		Lxip_local_file(Lxip::Socket_dir &p, Linux::socket &s)
		: Lxip_file(p, s, "local") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll(bool, Vfs::Vfs_handle::Context *) { return true; }

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			if (len < sizeof(_content_buffer))
				return -1;

			sockaddr_storage addr_storage;
			sockaddr_in *addr = (sockaddr_in *)&addr_storage;

			int out_len = sizeof(addr_storage);
			int const res = _sock.ops->getname(&_sock, (sockaddr *)addr, &out_len, 0);
			if (res < 0) return -1;

			in_addr const i_addr   = addr->sin_addr;
			unsigned char const *a = (unsigned char *)&i_addr.s_addr;
			unsigned char const *p = (unsigned char *)&addr->sin_port;
			return Genode::snprintf(dst, len,
			                        "%d.%d.%d.%d:%u\n",
			                        a[0], a[1], a[2], a[3], (p[0]<<8)|(p[1]<<0));
		}
};


class Vfs::Lxip_remote_file : public Vfs::Lxip_file
{
	public:

		Lxip_remote_file(Lxip::Socket_dir &p, Linux::socket &s)
		: Lxip_file(p, s, "remote") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll(bool trigger_io_response,
		          Vfs::Vfs_handle::Context *context) override
		{
			using namespace Linux;

			file f;
			f.f_flags = 0;

			switch (_parent.parent().type()) {
			case Lxip::Protocol_dir::TYPE_DGRAM:
				if (_sock.ops->poll(&f, &_sock, nullptr) & (POLLIN_SET)) {
					if (trigger_io_response)
						_parent.trigger_io_response(context);
					return true;
				}
				return false;

			case Lxip::Protocol_dir::TYPE_STREAM:
				if (trigger_io_response)
					_parent.trigger_io_response(context);
				return true;
			}

			return false;
		}

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			sockaddr_storage addr_storage;
			sockaddr_in *addr = (sockaddr_in *)&addr_storage;

			switch (_parent.parent().type()) {
			case Lxip::Protocol_dir::TYPE_DGRAM:
				{
					/* peek the sender address of the next packet */

					/* buffer not used */
					iovec iov { _content_buffer, sizeof(_content_buffer) };

					msghdr msg = create_msghdr(addr, sizeof(addr_storage),
					                           sizeof(_content_buffer), &iov);

					int const res = _sock.ops->recvmsg(&_sock, &msg, 0, MSG_DONTWAIT|MSG_PEEK);
					if (res == -EAGAIN) throw Would_block();
					if (res < 0)        return -1;
				}
				break;
			case Lxip::Protocol_dir::TYPE_STREAM:
				{
					int out_len = sizeof(addr_storage);
					int const res = _sock.ops->getname(&_sock, (sockaddr *)addr, &out_len, 1);
					if (res < 0) return -1;
				}
				break;
			}

			in_addr const i_addr   = addr->sin_addr;
			unsigned char const *a = (unsigned char *)&i_addr.s_addr;
			unsigned char const *p = (unsigned char *)&addr->sin_port;
			return Genode::snprintf(dst, len,
			                        "%d.%d.%d.%d:%u\n",
			                        a[0], a[1], a[2], a[3], (p[0]<<8)|(p[1]<<0));
		}

		Lxip::ssize_t write(char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_write_content_line(src, len)) return -1;

			long const port = get_port(_content_buffer);
			if (port == -1) return -1;

			sockaddr_in *remote_addr     = (sockaddr_in *)&_parent.remote_addr();
			remote_addr->sin_port        = htons(port);
			remote_addr->sin_addr.s_addr = get_addr(_content_buffer);
			remote_addr->sin_family      = AF_INET;

			return len;
		}
};


class Vfs::Lxip_accept_file : public Vfs::Lxip_file
{
	public:

		Lxip_accept_file(Lxip::Socket_dir &p, Linux::socket &s)
		: Lxip_file(p, s, "accept") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll(bool trigger_io_response,
		          Vfs::Vfs_handle::Context *context) override
		{
			using namespace Linux;

			file f;
			f.f_flags = 0;

			if (_sock.ops->poll(&f, &_sock, nullptr) & (POLLIN)) {
				if (trigger_io_response)
					_parent.trigger_io_response(context);
				return true;
			}
			return false;
		}

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			if (!poll(false, nullptr)) {
				throw Would_block();
			}

			socket *new_sock = sock_alloc();

			if (int res = _sock.ops->accept(&_sock, new_sock, O_NONBLOCK)) {
				kfree(new_sock);
				if (res == -EAGAIN) {
					throw Would_block();
				} else {
					return -1;
				}
			}
			set_sock_wait(new_sock, 0);

			new_sock->type = _sock.type;
			new_sock->ops  = _sock.ops;

			try {
				unsigned const id = _parent.accept(*new_sock);
				return Genode::snprintf(dst, len, "%s/%u\n", _parent.top_dir(), id);
			} catch (...) {
				Genode::error("lxip: could not adopt new client socket");
			}

			kfree(new_sock);
			return -1;
		}
};


class Vfs::Lxip_socket_dir final : public Vfs::Directory,
                                   public Lxip::Socket_dir
{
	public:

		enum {
			ACCEPT_NODE, BIND_NODE, CONNECT_NODE,
			DATA_NODE, LOCAL_NODE, LISTEN_NODE, REMOTE_NODE,
			MAX_NODES
		};

	private:

		Genode::Allocator        &_alloc;
		Lxip::Protocol_dir       &_parent;
		Vfs::Io_response_handler &_io_response_handler;
		Linux::socket            &_sock;

		Vfs::Node *_nodes[MAX_NODES];

		Linux::sockaddr_storage _remote_addr;

		unsigned _num_nodes()
		{
			unsigned num = 0;
			for (Vfs::Node *n : _nodes) num += (n != nullptr);
			return num;
		}

		Lxip_accept_file  _accept_file  { *this, _sock };
		Lxip_bind_file    _bind_file    { *this, _sock };
		Lxip_connect_file _connect_file { *this, _sock };
		Lxip_data_file    _data_file    { *this, _sock };
		Lxip_listen_file  _listen_file  { *this, _sock };
		Lxip_local_file   _local_file   { *this, _sock };
		Lxip_remote_file  _remote_file  { *this, _sock };

		char _name[Lxip::MAX_SOCKET_NAME_LEN];

	public:

		Lxip_socket_dir(Genode::Allocator &alloc, unsigned id,
		                Lxip::Protocol_dir &parent,
		                Vfs::Io_response_handler &io_response_handler,
		                Linux::socket &sock,
		                bool from_accept)
		:
			Directory(_name),
			_alloc(alloc), _parent(parent), _io_response_handler(io_response_handler),
			_sock(sock)
		{
			Genode::snprintf(_name, sizeof(_name), "%u", id);

			for (Vfs::Node * &node : _nodes) node = nullptr;

			_nodes[BIND_NODE]    = &_bind_file;
			_nodes[CONNECT_NODE] = &_connect_file;
			_nodes[DATA_NODE]    = &_data_file;
			_nodes[LOCAL_NODE]   = &_local_file;
			_nodes[REMOTE_NODE]  = &_remote_file;
		}

		~Lxip_socket_dir()
		{
			_accept_file.dissolve_handles();
			_bind_file.dissolve_handles();
			_connect_file.dissolve_handles();
			_data_file.dissolve_handles();
			_listen_file.dissolve_handles();
			_local_file.dissolve_handles();
			_remote_file.dissolve_handles();

			Linux::socket *sock = &_sock;

			if (sock->ops)
				sock->ops->release(sock);

			kfree(sock->wq);
			kfree(sock);
		}

		/**************************
		 ** Socket_dir interface **
		 **************************/

		Lxip::Protocol_dir &parent() override { return _parent; }

		Linux::sockaddr_storage &remote_addr() override { return _remote_addr; }

		char const *top_dir() override { return _parent.top_dir(); }

		unsigned accept(Linux::socket &sock) override
		{
			return _parent.adopt_socket(sock, true);
		}

		void bind(bool v) override
		{
			_nodes[LISTEN_NODE] = v ? &_listen_file : nullptr;
		}

		long bind() override { return _bind_file.port(); }

		bool lookup_port(long port) override { return _parent.lookup_port(port); }

		void connect(bool v) override
		{
			_nodes[REMOTE_NODE] = v ? &_remote_file : nullptr;
		}

		void listen(bool v) override
		{
			_nodes[ACCEPT_NODE] = v ? &_accept_file : nullptr;
		}

		bool _closed = false;

		void close()        override { _closed = true; }
		bool closed() const override { return _closed; }

		void trigger_io_response(Vfs::Vfs_handle::Context *context) override
		{
			_io_response_handler.handle_io_response(context);
		}

		/*************************
		 ** Directory interface **
		 *************************/

		Vfs::Node *child(char const *name) override
		{
			for (Vfs::Node *n : _nodes)
				if (n && Genode::strcmp(n->name(), name) == 0)
					return n;

			return nullptr;
		}

		file_size num_dirent() override { return _num_nodes(); }

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size seek_offset) override
		{
			typedef Vfs::Directory_service::Dirent Dirent;

			if (len < sizeof(Dirent))
				return -1;

			Vfs::file_size index = seek_offset / sizeof(Dirent);

			Dirent *out = (Dirent*)dst;

			out->fileno  = index+1;
			out->type    = Directory_service::DIRENT_TYPE_END;
			out->name[0] = '\0';

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
			if (!node) return -1;

			out->type = Directory_service::DIRENT_TYPE_FILE;

			strncpy(out->name, node->name(), sizeof(out->name));

			return sizeof(Dirent);
		}
};


class Vfs::Lxip_new_socket_file : public Vfs::File
{
	private:

		Lxip::Protocol_dir &_parent;

	public:

		Lxip_new_socket_file(Lxip::Protocol_dir &parent)
		: Vfs::File("new_socket"), _parent(parent) { }

		bool poll(bool, Vfs::Vfs_handle::Context *) override { return true; }

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			using namespace Linux;

			socket *sock = nullptr;

			int type = (_parent.type() == Lxip::Protocol_dir::TYPE_STREAM)
			         ? SOCK_STREAM : SOCK_DGRAM;

			if (sock_create_kern(nullptr, AF_INET, type, 0, &sock)) {
				kfree(sock);
				return -1;
			}
			set_sock_wait(sock, 0);

			/* XXX always allow UDP broadcast */
			if (type == SOCK_DGRAM) {
				int enable = 1;
				sock_setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&enable, sizeof(enable));
			}

			try {
				unsigned const id = _parent.adopt_socket(*sock, false);
				return Genode::snprintf(dst, len, "%s/%u\n", _parent.top_dir(), id);
			} catch (...) {
				Genode::error("lxip: could not adopt socket");
			}

			return -1;
		}
};


class Lxip::Protocol_dir_impl : public Protocol_dir,
                                public Vfs::Directory
{
	private:

		Genode::Allocator        &_alloc;
		Vfs::File_system         &_parent;
		Vfs::Io_response_handler &_io_response_handler;

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

		Vfs::Lxip_new_socket_file _new_socket_file { *this };

	public:

		Protocol_dir_impl(Genode::Allocator        &alloc,
		                  Vfs::File_system         &parent,
		                  Vfs::Io_response_handler &io_response_handler,
		                  char               const *name,
		                  Lxip::Protocol_dir::Type  type)
		:
			Directory(name),
			_alloc(alloc), _parent(parent), _io_response_handler(io_response_handler),
			_type(type)
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

		Type type() { return _type; }

		unsigned adopt_socket(Linux::socket &sock, bool from_accept)
		{
			Vfs::Node **node = _unused_node();
			if (!node) throw -1;

			unsigned const id = ((unsigned char*)node - (unsigned char*)_nodes)/sizeof(*_nodes);

			Vfs::Lxip_socket_dir *dir = new (&_alloc)
				Vfs::Lxip_socket_dir(_alloc, id, *this, _io_response_handler,
				                     sock, from_accept);

			*node = dir;
			return id;
		}

		bool lookup_port(long port)
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

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   Vfs::file_size seek_offset) override
		{
			typedef Vfs::Directory_service::Dirent Dirent;

			if (len < sizeof(Dirent))
				return -1;

			Vfs::file_size index = seek_offset / sizeof(Dirent);

			Dirent *out = (Dirent*)dst;

			out->fileno  = index+1;
			out->type    = Vfs::Directory_service::DIRENT_TYPE_END;
			out->name[0] = '\0';

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
			if (!node) return -1;

			if (dynamic_cast<Vfs::Directory*>(node))
				out->type = Vfs::Directory_service::DIRENT_TYPE_DIRECTORY;

			if (dynamic_cast<Vfs::File*>(node))
				out->type = Vfs::Directory_service::DIRENT_TYPE_FILE;

			Genode::strncpy(out->name, node->name(), sizeof(out->name));

			return sizeof(Dirent);
		}

		Vfs::Node *child(char const *name) override { return nullptr; }
};


class Vfs::Lxip_address_file : public Vfs::File
{
	private:

		unsigned int &_numeric_address;

	public:

		Lxip_address_file(char const *name, unsigned int &numeric_address)
		: Vfs::File(name), _numeric_address(numeric_address) { }

		bool poll(bool, Vfs::Vfs_handle::Context *) { return true; }

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			enum {
				MAX_ADDRESS_STRING_SIZE = sizeof("000.000.000.000\n")
			};

			Genode::String<MAX_ADDRESS_STRING_SIZE> address {
				Net::Ipv4_address(&_numeric_address)
			};

			Lxip::size_t n = min(len, strlen(address.string()));
			memcpy(dst, address.string(), n);
			if (n < len)
				dst[n++] = '\n';

			return n;
		}
};


class Vfs::Lxip_link_state_file : public Vfs::File
{
	private:

		bool &_numeric_link_state;

	public:

		Lxip_link_state_file(char const *name, bool &numeric_link_state)
		: Vfs::File(name), _numeric_link_state(numeric_link_state) { }

		bool poll(bool, Vfs::Vfs_handle::Context *) { return true; }

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			enum {
				MAX_LINK_STATE_STRING_SIZE = sizeof("down\n")
			};

			Genode::String<MAX_LINK_STATE_STRING_SIZE> link_state {
				_numeric_link_state ? "up" : "down"
			};

			Lxip::size_t n = min(len, strlen(link_state.string()));
			memcpy(dst, link_state.string(), n);
			if (n < len)
				dst[n++] = '\n';

			return n;
		}
};


extern "C" unsigned int ic_myaddr;
extern "C" unsigned int ic_netmask;
extern "C" unsigned int ic_gateway;
extern "C" unsigned int ic_nameservers[1];

extern bool ic_link_state;


/*******************************
 ** Filesystem implementation **
 *******************************/

class Vfs::Lxip_file_system : public Vfs::File_system,
                              public Vfs::Directory
{
	private:

		Genode::Entrypoint       &_ep;
		Genode::Allocator        &_alloc;
		Vfs::Io_response_handler &_io_response_handler;

		Lxip::Protocol_dir_impl _tcp_dir {
			_alloc, *this, _io_response_handler, "tcp", Lxip::Protocol_dir::TYPE_STREAM };
		Lxip::Protocol_dir_impl _udp_dir {
			_alloc, *this, _io_response_handler, "udp", Lxip::Protocol_dir::TYPE_DGRAM  };

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

		Read_result _read(Vfs::Vfs_handle *vfs_handle, char *dst,
		                  Vfs::file_size count,
		                  Vfs::file_size &out_count)
		{
			Vfs::Node &node =
				static_cast<Vfs::Lxip_vfs_handle *>(vfs_handle)->node;

			if (!count)
				Genode::error("zero read of ", node.name());

			if (count) {
				Lxip::ssize_t res = node.read(dst, count, vfs_handle->seek());
				if (res < 0) return READ_ERR_IO;

				out_count = res;
			}
			return READ_OK;
		}

	public:

		Lxip_file_system(Genode::Env &env, Genode::Allocator &alloc,
		                 Genode::Xml_node config,
	                     Vfs::Io_response_handler &io_response_handler)
		:
			Directory(""),
			_ep(env.ep()), _alloc(alloc), _io_response_handler(io_response_handler)
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
				lxip_configure_mtu(mtu);
			} else {
				lxip_configure_mtu(0);
			}

			if (config.attribute_value("dhcp", false)) {
				log("Using DHCP for interface configuration.");
				lxip_configure_dhcp();
				return;
			}

			try {

				Addr ip_addr    = config.attribute_value("ip_addr", Addr());
				Addr netmask    = config.attribute_value("netmask", Addr());
				Addr gateway    = config.attribute_value("gateway", Addr());
				Addr nameserver = config.attribute_value("nameserver", Addr());

				/* either none or all 4 interface attributes must exist */
				if (ip_addr == "") {
					warning("Missing \"ip_addr\" attribute. Ignoring network interface config.");
					throw Genode::Xml_node::Nonexistent_attribute();
				} else if (netmask == "") {
					warning("Missing \"netmask\" attribute. Ignoring network interface config.");
					throw Genode::Xml_node::Nonexistent_attribute();
				} else if (gateway == "") {
					warning("Missing \"gateway\" attribute. Ignoring network interface config.");
					throw Genode::Xml_node::Nonexistent_attribute();
				}

				log("static network interface: ip_addr=",ip_addr," netmask=",netmask," gateway=",gateway);

				lxip_configure_static(ip_addr.string(), netmask.string(),
				                      gateway.string(), nameserver.string());
			} catch (...) { }
		 }


		/*************************
		 ** Directory interface **
		 *************************/

		file_size num_dirent() override { return 7; }

		Lxip::ssize_t read(char *dst, Genode::size_t len,
		                   file_size seek_offset) override
		{
			if (len < sizeof(Dirent))
				return -1;

			file_size index = seek_offset / sizeof(Dirent);

			Dirent *out = (Dirent*)dst;

			if (index == 0) {
				out->fileno  = (Genode::addr_t)&_tcp_dir;
				out->type    = Directory_service::DIRENT_TYPE_DIRECTORY;
				Genode::strncpy(out->name, "tcp", sizeof(out->name));
			} else if (index == 1) {
				out->fileno  = (Genode::addr_t)&_udp_dir;
				out->type    = Directory_service::DIRENT_TYPE_DIRECTORY;
				Genode::strncpy(out->name, "udp", sizeof(out->name));
			} else if (index == 2) {
				out->fileno  = (Genode::addr_t)&_address;
				out->type    = Directory_service::DIRENT_TYPE_FILE;
				Genode::strncpy(out->name, "address", sizeof(out->name));
			} else if (index == 3) {
				out->fileno  = (Genode::addr_t)&_netmask;
				out->type    = Directory_service::DIRENT_TYPE_FILE;
				Genode::strncpy(out->name, "netmask", sizeof(out->name));
			} else if (index == 4) {
				out->fileno  = (Genode::addr_t)&_gateway;
				out->type    = Directory_service::DIRENT_TYPE_FILE;
				Genode::strncpy(out->name, "gateway", sizeof(out->name));
			} else if (index == 5) {
				out->fileno  = (Genode::addr_t)&_nameserver;
				out->type    = Directory_service::DIRENT_TYPE_FILE;
				Genode::strncpy(out->name, "nameserver", sizeof(out->name));
			} else if (index == 6) {
				out->fileno  = (Genode::addr_t)&_link_state;
				out->type    = Directory_service::DIRENT_TYPE_FILE;
				Genode::strncpy(out->name, "link_state", sizeof(out->name));
			} else {
				out->fileno  = 0;
				out->type    = Directory_service::DIRENT_TYPE_END;
				out->name[0] = '\0';
			}

			return sizeof(Dirent);
		}

		Vfs::Node *child(char const *name) override { return nullptr; }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path) override {
			return Dataspace_capability(); }

		void release(char const *path, Dataspace_capability ds_cap) override { }

		Stat_result stat(char const *path, Stat &out) override
		{
			Vfs::Node *node = _lookup(path);
			if (!node) return STAT_ERR_NO_ENTRY;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (dir) {
				out.mode = STAT_MODE_DIRECTORY | 0777;
				return STAT_OK;
			}

			if (dynamic_cast<Lxip_file*>(node)) {
				out.mode = STAT_MODE_FILE | 0666;
				out.size = 0;
				return STAT_OK;
			}

			if (dynamic_cast<Vfs::File*>(node)) {
				out.mode = STAT_MODE_FILE | 0666;
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
			return path;
		}

		Open_result open(char const *path, unsigned mode,
		                 Vfs_handle **out_handle,
		                 Genode::Allocator &alloc) override
		{
			if (mode & OPEN_MODE_CREATE) return OPEN_ERR_NO_PERM;

			Vfs::Node *node = _lookup(path);
			if (!node) return OPEN_ERR_UNACCESSIBLE;

			Vfs::File *file = dynamic_cast<Vfs::File*>(node);
			if (file) {
				Lxip_vfs_file_handle *handle =
					new (alloc) Vfs::Lxip_vfs_file_handle(*this, alloc, 0, *file);
				*out_handle = handle;

				return OPEN_OK;
			}

			return OPEN_ERR_UNACCESSIBLE;
		}

		Opendir_result opendir(char const *path, bool create,
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

			if (!handle)
				return;

			Lxip_vfs_file_handle *file_handle =
				dynamic_cast<Vfs::Lxip_vfs_file_handle*>(handle);

			if (file_handle)
				_polling_handles.remove(&file_handle->polling_le);

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

		Write_result write(Vfs_handle *vfs_handle, char const *src,
		                   file_size count,
		                   file_size &out_count) override
		{
			Vfs::File &file =
				static_cast<Vfs::Lxip_vfs_file_handle *>(vfs_handle)->file;

			if (!count)
				Genode::error("zero write of ",file.name());

			if (count) try {
				Lxip::ssize_t res = file.write(src, count, vfs_handle->seek());
				if (res < 0) return WRITE_ERR_IO;

				out_count = res;

			} catch (File::Would_block) { return WRITE_ERR_WOULD_BLOCK; }
			return WRITE_OK;
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          char *dst, file_size count,
		                          file_size &out_count) override
		{
			try { return _read(vfs_handle, dst, count, out_count); }
			catch (File::Would_block) { return READ_QUEUED; }
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			/* report ok because libc always executes ftruncate() when opening rw */
			return FTRUNCATE_OK;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			Lxip_vfs_file_handle *handle =
				static_cast<Vfs::Lxip_vfs_file_handle *>(vfs_handle);

			if (dynamic_cast<Lxip_file*>(&handle->file)) {
				_polling_handles.remove(&handle->polling_le);
				_polling_handles.insert(&handle->polling_le);
			}

			return true;
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			Lxip_vfs_file_handle *handle =
				static_cast<Vfs::Lxip_vfs_file_handle *>(vfs_handle);

			return handle->file.poll(false, nullptr);
		}
};


struct Lxip_factory : Vfs::File_system_factory
{
	struct Init
	{
		char _config_buf[128];

		char *_parse_config(Genode::Xml_node);

		Init(Genode::Env       &env,
		     Genode::Allocator &alloc)
		{
			Lx_kit::Env &lx_env = Lx_kit::construct_env(env);

			Lx::lxcc_emul_init(lx_env);
			Lx::malloc_init(env, lx_env.heap());
			Lx::timer_init(env, lx_env.env().ep(), lx_env.heap(), &poll_all);
			Lx::event_init(env, lx_env.env().ep(), &poll_all);
			Lx::nic_client_init(env, lx_env.env().ep(), lx_env.heap(), &poll_all);

			lxip_init();
		}
	};

	Vfs::File_system *create(Genode::Env       &env,
	                         Genode::Allocator &alloc,
	                         Genode::Xml_node  config,
	                         Vfs::Io_response_handler &io_handler) override
	{
		static Init inst(env, alloc);
		return new (alloc) Vfs::Lxip_file_system(env, alloc, config, io_handler);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Lxip_factory factory;
	return &factory;
}

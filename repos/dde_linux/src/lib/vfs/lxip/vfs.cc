/*
 * \brief  lxip-based socket file system
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
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

	virtual Lxip::ssize_t write(Lxip_vfs_file_handle &,
	                             char const *src, Genode::size_t len,
	                             file_size)
	{
		Genode::error(name(), " not writeable");
		return -1;
	}

	virtual Lxip::ssize_t  read(Lxip_vfs_file_handle &,
	                            char *dst, Genode::size_t len,
	                            file_size)
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
	                         Vfs::Io_response_handler &io_handler,
	                         Genode::Allocator &alloc,
	                         char const*, unsigned, Vfs::Vfs_handle**) = 0;

	virtual Lxip::ssize_t read(char *dst, Genode::size_t len,
	                           file_size seek_offset) = 0;
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
	virtual sockaddr_storage &remote_addr() = 0;
	virtual void     close() = 0;
	virtual bool     closed() const = 0;
	virtual void trigger_io_response(Vfs::Vfs_handle::Context *) = 0;

	Socket_dir(char const *name) : Vfs::Directory(name) { }
};


struct Vfs::Lxip_vfs_handle : Vfs::Vfs_handle
{
	typedef File_io_service:: Read_result Read_result;
	typedef File_io_service::Write_result Write_result;
	typedef File_io_service::Sync_result Sync_result;

	Lxip_vfs_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags)
	: Vfs::Vfs_handle(fs, fs, alloc, status_flags) { }

	virtual bool read_ready() = 0;

	virtual Read_result   read(char *dst,
	                           file_size count, file_size &out_count) = 0;
	virtual Write_result write(char const *src,
	                           file_size count, file_size &out_count) = 0;

	virtual Sync_result sync() {
		return Sync_result::SYNC_OK; }
};


struct Vfs::Lxip_vfs_file_handle final : Vfs::Lxip_vfs_handle
{
	Lxip_vfs_file_handle(Lxip_vfs_file_handle const &);
	Lxip_vfs_file_handle &operator = (Lxip_vfs_file_handle const &);

	Vfs::File *file;

	List_element<Lxip_vfs_file_handle> file_le    { this };
	List_element<Lxip_vfs_file_handle> polling_le { this };

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

	bool read_ready() override {
		return (file) ? file->poll(false, nullptr) : false; }

	Read_result read(char *dst, file_size count, file_size &out_count) override
	{
		if (!file) return Read_result::READ_ERR_INVALID;
		Lxip::ssize_t res = file->read(*this, dst, count, seek());
		if (res < 0) return Read_result::READ_ERR_IO;
		out_count = res;
		return Read_result::READ_OK;
	}

	Write_result write(char const *src, file_size count, file_size &out_count) override
	{
		if (!file) return Write_result::WRITE_ERR_INVALID;
		Lxip::ssize_t res = file->write(*this, src, count, seek());
		if (res < 0) return Write_result::WRITE_ERR_IO;
		out_count = res;
		return Write_result::WRITE_OK;
	}

	bool write_content_line(char const *buf, Genode::size_t len)
	{
		if (len > sizeof(content_buffer) - 2)
			return false;

		Genode::memcpy(content_buffer, buf, len);
		content_buffer[len+0] = '\n';
		content_buffer[len+1] = '\0';
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

	bool read_ready() override { return true; }

	Read_result read(char *dst, file_size count, file_size &out_count) override
	{
		Lxip::ssize_t res = dir.read(dst, count, seek());
		if (res < 0) return Read_result::READ_ERR_IO;
		out_count = res;
		return Read_result::READ_OK;
	}

	Write_result write(char const*, file_size, file_size &) override {
		return Write_result::WRITE_ERR_INVALID; }
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
		if (handle->file)
			handle->file->poll(true, handle->context);
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

		int _write_err = 0;

		bool _sock_valid() { return _sock.sk != nullptr; }

	public:

		Lxip_file(Lxip::Socket_dir &p, Linux::socket &s, char const *name)
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
				_polling_handles.remove(&h->polling_le);
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

		Lxip::ssize_t write(Lxip_vfs_file_handle &,
		                    char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			iovec iov { const_cast<char *>(src), len };

			msghdr msg = create_msghdr(&_parent.remote_addr(),
			                           sizeof(sockaddr_in), len, &iov);

			Lxip::ssize_t res = _sock.ops->sendmsg(&_sock, &msg, len);

			if (res < 0) _write_err = res;

			return res;
		}

		Lxip::ssize_t read(Lxip_vfs_file_handle &,
		                   char *dst, Genode::size_t len,
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

		Lxip::ssize_t write(Lxip_vfs_file_handle &handle,
		                    char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			/* already bound to port */
			if (_port >= 0) return -1;

			if (!handle.write_content_line(src, len)) return -1;

			/* check if port is already used by other socket */
			long port = get_port(handle.content_buffer);
			if (port == -1)                return -1;
			if (_parent.lookup_port(port)) return -1;

			/* port is free, try to bind it */
			sockaddr_storage addr_storage;

			sockaddr_in *addr     = (sockaddr_in *)&addr_storage;
			addr->sin_port        = htons(port);
			addr->sin_addr.s_addr = get_addr(handle.content_buffer);
			addr->sin_family      = AF_INET;

			_write_err = _sock.ops->bind(&_sock, (sockaddr*)addr, sizeof(addr_storage));
			if (_write_err != 0) return -1;

			_port = port;

			_parent.bind(true);

			return len;
		}

		Lxip::ssize_t read(Lxip_vfs_file_handle &handle,
		                   char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			if (len < sizeof(handle.content_buffer))
				return -1;

			Genode::size_t const n = Genode::strlen(handle.content_buffer);
			Genode::memcpy(dst, handle.content_buffer, n);

			return n;
		}
};


class Vfs::Lxip_listen_file : public Vfs::Lxip_file
{
	private:

		unsigned long _backlog = ~0UL;

	public:

		Lxip_listen_file(Lxip::Socket_dir &p, Linux::socket &s)
		: Lxip_file(p, s, "listen") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll(bool, Vfs::Vfs_handle::Context *) { return true; }

		Lxip::ssize_t write(Lxip_vfs_file_handle &handle,
		                    char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			if (!_sock_valid()) return -1;

			/* write-once */
			if (_backlog != ~0UL) return -1;

			if (!handle.write_content_line(src, len)) return -1;

			Genode::ascii_to_unsigned(
				handle.content_buffer, _backlog, sizeof(handle.content_buffer));

			if (_backlog == ~0UL) return -1;

			_write_err = _sock.ops->listen(&_sock, _backlog);
			if (_write_err != 0) {
				handle.write_content_line("", 0);
				return -1;
			}

			_parent.listen(true);

			return len;
		}

		Lxip::ssize_t read(Lxip_vfs_file_handle &handle,
		                   char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			return Genode::snprintf(dst, len, "%lu\n", _backlog);
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

		Lxip::ssize_t write(Lxip_vfs_file_handle &handle,
		                    char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			if (!handle.write_content_line(src, len)) return -1;

			long const port = get_port(handle.content_buffer);
			if (port == -1) return -1;

			sockaddr_storage addr_storage;

			sockaddr_in *addr     = (sockaddr_in *)&addr_storage;
			addr->sin_port        = htons(port);
			addr->sin_addr.s_addr = get_addr(handle.content_buffer);
			addr->sin_family      = AF_INET;

			_write_err = _sock.ops->connect(&_sock, (sockaddr *)addr, sizeof(addr_storage), 0);

			switch (_write_err) {
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
				if (_write_err != 0) return -1;
				break;
			}

			sockaddr_in *remote_addr     = (sockaddr_in *)&_parent.remote_addr();
			remote_addr->sin_port        = htons(port);
			remote_addr->sin_addr.s_addr = get_addr(handle.content_buffer);
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

		Lxip::ssize_t read(Lxip_vfs_file_handle &handle,
		                   char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			if (len < sizeof(handle.content_buffer))
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

		Lxip::ssize_t read(Lxip_vfs_file_handle &handle,
		                   char *dst, Genode::size_t len,
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
					iovec iov { handle.content_buffer, sizeof(handle.content_buffer) };

					msghdr msg = create_msghdr(addr, sizeof(addr_storage),
					                           sizeof(handle.content_buffer), &iov);

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

		Lxip::ssize_t write(Lxip_vfs_file_handle &handle,
		                    char const *src, Genode::size_t len,
		                    file_size /* ignored */) override
		{
			using namespace Linux;

			if (!handle.write_content_line(src, len)) return -1;

			long const port = get_port(handle.content_buffer);
			if (port == -1) return -1;

			sockaddr_in *remote_addr     = (sockaddr_in *)&_parent.remote_addr();
			remote_addr->sin_port        = htons(port);
			remote_addr->sin_addr.s_addr = get_addr(handle.content_buffer);
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

		Lxip::ssize_t read(Lxip_vfs_file_handle &handle,
		                   char *dst, Genode::size_t len,
		                   file_size /* ignored */) override
		{
			using namespace Linux;

			if (!_sock_valid()) return -1;

			file f;
			f.f_flags = 0;

			if (_sock.ops->poll(&f, &_sock, nullptr) & (POLLIN)) {
				Genode::strncpy(dst, "1\n", len);
				return Genode::strlen(dst);
			}
			return 0;
		}
};


class Vfs::Lxip_socket_dir final : public Lxip::Socket_dir
{
	public:

		enum {
			ACCEPT_NODE, BIND_NODE, CONNECT_NODE,
			DATA_NODE, LOCAL_NODE, LISTEN_NODE, REMOTE_NODE,
			ACCEPT_SOCKET_NODE,
			MAX_FILES
		};

	private:

		Genode::Allocator        &_alloc;
		Lxip::Protocol_dir       &_parent;
		Vfs::Io_response_handler &_io_response_handler;
		Linux::socket            &_sock;

		Vfs::File *_files[MAX_FILES];

		Linux::sockaddr_storage _remote_addr;

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
		Lxip_listen_file  _listen_file  { *this, _sock };
		Lxip_local_file   _local_file   { *this, _sock };
		Lxip_remote_file  _remote_file  { *this, _sock };

		struct Accept_socket_file : Vfs::File
		{
			Accept_socket_file() : Vfs::File("accept_socket") { }

			bool poll(bool, Vfs::Vfs_handle::Context *) override { return true; }
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
		                Vfs::Io_response_handler &io_response_handler,
		                Linux::socket &sock)
		:
			Lxip::Socket_dir(_name),
			_alloc(alloc), _parent(parent), _io_response_handler(io_response_handler),
			_sock(sock), id(parent.adopt_socket(*this))
		{
			Genode::snprintf(_name, sizeof(_name), "%u", id);

			for (Vfs::File * &file : _files) file = nullptr;

			_files[BIND_NODE]    = &_bind_file;
			_files[CONNECT_NODE] = &_connect_file;
			_files[DATA_NODE]    = &_data_file;
			_files[LOCAL_NODE]   = &_local_file;
			_files[REMOTE_NODE]  = &_remote_file;
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

			_parent.release(id);
		}

		/**************************
		 ** Socket_dir interface **
		 **************************/

		Lxip::Protocol_dir &parent() override { return _parent; }

		Linux::sockaddr_storage &remote_addr() override { return _remote_addr; }

		char const *top_dir() override { return _parent.top_dir(); }

		Open_result
		open(Vfs::File_system &fs,
		     Vfs::Io_response_handler &,
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

		void bind(bool v) override
		{
			_files[LISTEN_NODE] = v ? &_listen_file : nullptr;
		}

		long bind() override { return _bind_file.port(); }

		bool lookup_port(long port) override { return _parent.lookup_port(port); }

		void connect(bool v) override
		{
			_files[REMOTE_NODE] = v ? &_remote_file : nullptr;
		}

		void listen(bool v) override
		{
			_files[ACCEPT_NODE] = v ? &_accept_file : nullptr;
			_files[ACCEPT_SOCKET_NODE] = v ? &_accept_socket_file : nullptr;
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
			for (Vfs::File *n : _files)
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
			for (Vfs::File *n : _files) {
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


struct Vfs::Lxip_socket_handle final : Vfs::Lxip_vfs_handle
{
		Lxip_socket_dir socket_dir;

		Lxip_socket_handle(Vfs::File_system &fs,
		                   Vfs::Io_response_handler &io_handler,
		                   Genode::Allocator &alloc,
		                   Lxip::Protocol_dir &parent,
		                   Linux::socket &sock)
		:
			Lxip_vfs_handle(fs, alloc, 0),
			socket_dir(alloc, parent, io_handler, sock)
		{ }

		bool read_ready() override { return true; }

		Read_result read(char *dst, file_size count, file_size &out_count) override
		{
			out_count = Genode::snprintf(
				dst, count, "%s/%s\n", socket_dir.parent().name(), socket_dir.name());
			return Read_result::READ_OK;
		}

		Write_result write(char const *src, file_size count, file_size &out_count) override {
			return Write_result::WRITE_ERR_INVALID; }
};


Vfs::Directory_service::Open_result
Vfs::Lxip_socket_dir::_accept_new_socket(Vfs::File_system &fs,
                                         Genode::Allocator &alloc,
                                         Vfs::Vfs_handle **out_handle)
{
	using namespace Linux;

	Open_result res = Open_result::OPEN_ERR_UNACCESSIBLE;
	if (!_files[ACCEPT_SOCKET_NODE]) return res;

	socket *new_sock = sock_alloc();

	if (_sock.ops->accept(&_sock, new_sock, O_NONBLOCK)) {
		error("accept socket failed");
		kfree(new_sock);
		return res;
	}

	set_sock_wait(new_sock, 0);
	new_sock->type = _sock.type;
	new_sock->ops  = _sock.ops;

	try {
		Vfs::Lxip_socket_handle *handle = new (alloc)
			Vfs::Lxip_socket_handle(fs, _io_response_handler, alloc,
			                        _parent, *new_sock);
		*out_handle = handle;
		return Vfs::Directory_service::Open_result::OPEN_OK;
	}

	catch (Genode::Out_of_ram)  { res = Open_result::OPEN_ERR_OUT_OF_RAM;  }
	catch (Genode::Out_of_caps) { res = Open_result::OPEN_ERR_OUT_OF_CAPS; }
	catch (...) { Genode::error("unhandle error during accept"); }

	kfree(new_sock);
	return res;
};


class Lxip::Protocol_dir_impl : public Protocol_dir
{
	private:

		Genode::Allocator        &_alloc;
		Vfs::File_system         &_parent;
		Vfs::Io_response_handler &_io_response_handler;

		struct New_socket_file : Vfs::File
		{
			New_socket_file() : Vfs::File("new_socket") { }

			bool poll(bool, Vfs::Vfs_handle::Context *) override { return true; }
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
		                 Vfs::Io_response_handler &io_handler,
		                 Genode::Allocator &alloc,
		                 Vfs::Vfs_handle **out_handle)
		{
			using namespace Linux;

			Vfs::Directory_service::Open_result res =
				Vfs::Directory_service::Open_result::OPEN_ERR_UNACCESSIBLE;

			socket *sock = nullptr;

			int type = (_type == Lxip::Protocol_dir::TYPE_STREAM)
			         ? SOCK_STREAM : SOCK_DGRAM;

			if (sock_create_kern(nullptr, AF_INET, type, 0, &sock)) {
				kfree(sock);
				return res;
			}

			set_sock_wait(sock, 0);

			/* XXX always allow UDP broadcast */
			if (type == SOCK_DGRAM) {
				int enable = 1;
				sock_setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&enable, sizeof(enable));
			}

			try {
				Vfs::Lxip_socket_handle *handle = new (alloc)
					Vfs::Lxip_socket_handle(fs, io_handler, alloc,
					                        *this, *sock);
				*out_handle = handle;
				return Vfs::Directory_service::Open_result::OPEN_OK;
			}

			catch (Genode::Out_of_ram)  {
				res = Open_result::OPEN_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) {
				res = Open_result::OPEN_ERR_OUT_OF_CAPS; }
			catch (...) { Genode::error("unhandle error during accept"); }

			kfree(sock);
			return res;
		}

	public:

		Protocol_dir_impl(Genode::Allocator        &alloc,
		                  Vfs::File_system         &parent,
		                  Vfs::Io_response_handler &io_response_handler,
		                  char               const *name,
		                  Lxip::Protocol_dir::Type  type)
		:
			Protocol_dir(name),
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

		Open_result open(Vfs::File_system &fs,
		                 Vfs::Io_response_handler &io_handler,
		                 Genode::Allocator &alloc,
		                 char const *path, unsigned mode,
		                 Vfs::Vfs_handle **out_handle) override
		{
			if (strcmp(path, "/new_socket") == 0) {
				if (mode != 0) return Open_result::OPEN_ERR_NO_PERM;
				return _open_new_socket(fs, io_handler, alloc, out_handle);
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
						return dir->open(fs, io_handler, alloc, path, mode, out_handle);
					}
				}
			}

			return Open_result::OPEN_ERR_UNACCESSIBLE;
		}

		unsigned adopt_socket(Lxip::Socket_dir &dir)
		{
			Vfs::Node **node = _unused_node();
			if (!node) throw -1;

			unsigned const id = ((unsigned char*)node - (unsigned char*)_nodes)/sizeof(*_nodes);

			*node = &dir;
			return id;
		}

		void release(unsigned id)
		{
			if (id < MAX_NODES)
				_nodes[id] = nullptr;
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

		Lxip::ssize_t read(Lxip_vfs_file_handle &handle,
		                   char *dst, Genode::size_t len,
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

		Lxip::ssize_t read(Lxip_vfs_file_handle &handle,
		                   char *dst, Genode::size_t len,
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
			Vfs::Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);

			return handle->read(dst, count, out_count);
		}

	public:

		Lxip_file_system(Vfs::Env &env, Genode::Xml_node config)
		:
			Directory(""),
			_ep(env.env().ep()), _alloc(env.alloc()),
			_io_response_handler(env.io_handler())
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

				if (ip_addr == "") {
					warning("Missing \"ip_addr\" attribute. Ignoring network interface config.");
					throw Genode::Xml_node::Nonexistent_attribute();
				} else if (netmask == "") {
					warning("Missing \"netmask\" attribute. Ignoring network interface config.");
					throw Genode::Xml_node::Nonexistent_attribute();
				}

				log("static network interface: ip_addr=",ip_addr," netmask=",netmask);

				lxip_configure_static(ip_addr.string(), netmask.string(),
				                      gateway.string(), nameserver.string());
			} catch (...) { }
		 }


		/*************************
		 ** Directory interface **
		 *************************/

		file_size num_dirent() override { return 7; }

		Vfs::Directory::Open_result
		open(Vfs::File_system &fs,
	         Vfs::Io_response_handler &io_handler,
	         Genode::Allocator &alloc,
	         char const*, unsigned, Vfs::Vfs_handle**) override {
			return Vfs::Directory::Open_result::OPEN_ERR_UNACCESSIBLE; }

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
					return _tcp_dir.open(*this, _io_response_handler, alloc,
					                     &path[4], mode, out_handle);
				if (Genode::strcmp(path, "/udp", 4) == 0)
					return _udp_dir.open(*this, _io_response_handler, alloc,
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
			Vfs::Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);

			try { return handle->write(src, count, out_count); }
			catch (File::Would_block) { return WRITE_ERR_WOULD_BLOCK; }

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
				dynamic_cast<Vfs::Lxip_vfs_file_handle *>(vfs_handle);

			if (handle && dynamic_cast<Lxip_file*>(handle->file)) {
				_polling_handles.remove(&handle->polling_le);
				_polling_handles.insert(&handle->polling_le);
				return true;
			}

			return false;
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			Lxip_vfs_handle &handle =
				*static_cast<Lxip_vfs_handle *>(vfs_handle);
			return handle.read_ready();
		}

		Sync_result complete_sync(Vfs_handle *vfs_handle)
		{
			Vfs::Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);
			return handle->sync();
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

	Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
	{
		static Init inst(env.env(), env.alloc());
		return new (env.alloc()) Vfs::Lxip_file_system(env, config);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Lxip_factory factory;
	return &factory;
}

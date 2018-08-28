/*
 * \brief  LwIP VFS plugin
 * \author Emery Hemingway
 * \date   2016-09-27
 */

/*
 * Copyright (C) 2016-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vfs/directory_service.h>
#include <vfs/file_io_service.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>
#include <vfs/print.h>
#include <timer_session/connection.h>
#include <util/fifo.h>
#include <base/tslab.h>
#include <base/registry.h>
#include <base/log.h>

/* LwIP includes */
#include <lwip/genode_init.h>
#include <lwip/nic_netif.h>

namespace Lwip {
extern "C" {
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/dns.h>
}

	using namespace Vfs;

	typedef Vfs::File_io_service::Read_result Read_result;
	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Sync_result Sync_result;

	typedef Genode::String<8> Socket_name;

	char const *get_port(char const *s)
	{
		char const *p = s;
		while (*++p) {
			if (*p == ':')
				return ++p;
		}
		return nullptr;
	}

	int remove_port(char *p)
	{
		long tmp = -1;

		while (*++p)
			if (*p == ':') {
				*p++ = '\0';
				Genode::ascii_to_unsigned(p, tmp, 10);
				break;
			}

		return tmp;
	}

	class Socket_dir;
	class Udp_socket_dir;
	class Tcp_socket_dir;

	#define Udp_socket_dir_list Genode::List<Udp_socket_dir>
	#define Tcp_socket_dir_list Genode::List<Tcp_socket_dir>

	struct Protocol_dir;
	template <typename, typename> class Protocol_dir_impl;

	enum {
		MAX_SOCKETS         = 128,      /* 3 */
		MAX_SOCKET_NAME_LEN = 3 + 1,    /* + \0 */
		MAX_FD_STR_LEN      = 3 + 1 +1, /* + \n + \0 */
		MAX_DATA_LEN        = 32,       /* 255.255.255.255:65536 + something */
	};
	struct Lwip_handle;
	struct Lwip_nameserver_handle;
	struct Lwip_file_handle;
	struct Lwip_dir_handle;

	typedef Genode::Registry<Lwip_nameserver_handle> Nameserver_registry;

	#define Lwip_handle_list List<Lwip_file_handle>

	class File_system;

	typedef Vfs::Directory_service::Open_result    Open_result;
	typedef Vfs::Directory_service::Opendir_result Opendir_result;
	typedef Vfs::Directory_service::Unlink_result  Unlink_result;

	extern "C" {
		static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
		static err_t tcp_connect_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
		static err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
		static err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb,
		                               struct pbuf *p, err_t err);
		static err_t tcp_delayed_recv_callback(void *arg, struct tcp_pcb *tpcb,
		                                       struct pbuf *p, err_t err);
		/* static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len); */
		static void  tcp_err_callback(void *arg, err_t err);
	}

	typedef Genode::Path<24> Path;

	enum {
		PORT_STRLEN_MAX = 6, /* :65536 */
		ENDPOINT_STRLEN_MAX = IPADDR_STRLEN_MAX+PORT_STRLEN_MAX
	};

	struct Directory;
}


/**
 * Synthetic directory interface
 */
struct Lwip::Directory
{
	virtual ~Directory() { }

	virtual Read_result readdir(char *dst, file_size count,
	                            file_size &out_count) = 0;

	virtual bool directory(char const *path) = 0;
};


struct Lwip::Lwip_handle : Vfs::Vfs_handle
{
	Lwip_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags)
	: Vfs_handle(fs, fs, alloc, status_flags) { }

	virtual Read_result read(char *dst, file_size count,
	                         file_size &out_count) = 0;

	virtual Write_result write(char const *, file_size,
	                           file_size &) {
		return Write_result::WRITE_ERR_INVALID; }
};


struct Lwip::Lwip_dir_handle final : Lwip_handle
{
	/*
	  * Noncopyable
	 */
	Lwip_dir_handle(Lwip_dir_handle const &);
	Lwip_dir_handle &operator = (Lwip_dir_handle const &);

	Directory *dir;

	void print(Genode::Output &output) const;

	Lwip_dir_handle(Vfs::File_system &fs, Allocator &alloc, Directory &dir)
	: Lwip_handle(fs, alloc, 0), dir(&dir) { }

	Read_result read(char *dst, file_size count,
	                 file_size &out_count) override
	{
		return (dir)
			? dir->readdir(dst, count, out_count)
			: Read_result::READ_ERR_INVALID;
	}
};


struct Lwip::Lwip_nameserver_handle final : Lwip_handle, private Nameserver_registry::Element
{
	Lwip_nameserver_handle(Vfs::File_system &fs, Allocator &alloc, Nameserver_registry &registry)
	: Lwip_handle(fs, alloc, Vfs::Directory_service::OPEN_MODE_RDONLY),
	  Nameserver_registry::Element(registry, *this) { }

	Read_result read(char *dst, file_size count,
	                 file_size &out_count) override
	{
		memset(dst, 0x00, min(file_size(IPADDR_STRLEN_MAX), count));
		ipaddr_ntoa_r(dns_getserver(0), dst, count);

		auto n = strlen(dst);
		if (n < count)
			dst[n] = '\n';

		out_count = n+1;
		return Read_result::READ_OK;
	}
};


struct Lwip::Lwip_file_handle final : Lwip_handle, private Lwip_handle_list::Element
{
	friend class Lwip_handle_list;
	friend class Lwip_handle_list::Element;
	using Lwip_handle_list::Element::next;

	enum Kind {
		INVALID  = 0,
		ACCEPT   = 1 << 0,
		BIND     = 1 << 1,
		CONNECT  = 1 << 2,
		DATA     = 1 << 3,
		LISTEN   = 1 << 4,
		LOCAL    = 1 << 5,
		PEEK     = 1 << 6,
		REMOTE   = 1 << 7,
		LOCATION = 1 << 8,
		PENDING  = 1 << 9,
	};

	enum { DATA_READY = DATA | PEEK };

	/*
	  * Noncopyable
	 */
	Lwip_file_handle(Lwip_file_handle const &);
	Lwip_file_handle &operator = (Lwip_file_handle const &);

	static Kind kind_from_name(Path const &p)
	{
		if (p == "/accept_socket") return PENDING;
		if (p == "/accept")   return ACCEPT;
		if (p == "/bind")     return BIND;
		if (p == "/connect")  return CONNECT;
		if (p == "/data")     return DATA;
		if (p == "/listen")   return LISTEN;
		if (p == "/local")    return LOCAL;
		if (p == "/peek")     return PEEK;
		if (p == "/remote")   return REMOTE;
		return INVALID;
	}

	Socket_dir *socket;

	int in_transit = 0;

	Kind kind;

	bool notify = false;

	void print(Genode::Output &output) const;

	Lwip_file_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                 Socket_dir &s, Kind k);

	~Lwip_file_handle();

	Read_result read(char *dst, file_size count,
	                 file_size &out_count) override;

	Write_result write(char const *src, file_size count,
	                   file_size &out_count) override;
};


struct Lwip::Socket_dir : Lwip::Directory
{
		friend class Lwip_handle;
		friend class Lwip_handle_list;
		friend class Lwip_handle_list::Element;

		static Socket_name name_from_num(unsigned num)
		{
			char buf[Socket_name::capacity()];
			return Socket_name(Genode::Cstring(
				buf, Genode::snprintf(buf, Socket_name::capacity(), "%x", num)));
		}

		Genode::Allocator &alloc;

		Vfs::Io_response_handler &io_handler;

		unsigned    const _num;
		Socket_name const _name { name_from_num(_num) };

		/* lists of handles opened at this socket */
		Lwip_handle_list handles { };

		enum State {
			NEW,
			BOUND,
			CONNECT,
			LISTEN,
			READY,
			CLOSING,
			CLOSED
		};

		Socket_dir(unsigned num, Genode::Allocator &alloc, Vfs::Io_response_handler &io_handler)
		: alloc(alloc), io_handler(io_handler), _num(num) { };


		~Socket_dir()
		{
			/*
			 * Remove socket from handles
			 */
			while (Lwip_file_handle *handle = handles.first()) {
				handle->socket = nullptr;
				handles.remove(handle);
			}
		}

		Socket_name const &name() const { return _name; }

		bool operator == (unsigned other) const {
			return _num == other; }

		bool operator == (char const *other) const {
			return _name == other; }

		virtual Open_result _accept_new_socket(Vfs::File_system &,
                                               Genode::Allocator &,
                                               Vfs::Vfs_handle **) = 0;

		Open_result open(Vfs::File_system &fs,
		                 Path const  &name,
		                 unsigned     mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc)
		{
			Lwip_file_handle::Kind kind = Lwip_file_handle::kind_from_name(name);
			if (kind == Lwip_file_handle::INVALID)
				return Open_result::OPEN_ERR_UNACCESSIBLE;

			if (kind == Lwip_file_handle::LOCATION || kind == Lwip_file_handle::PENDING)
				return _accept_new_socket(fs, alloc, out_handle);

			*out_handle = new (alloc) Lwip_file_handle(fs, alloc, mode, *this, kind);
			return Open_result::OPEN_OK;
		}

		Read_result readdir(char *dst, file_size count,
		                    file_size &out_count) override
		{
			Genode::warning(__func__, " NOT_IMPLEMENTED");
			(void)dst;
			(void)count;
			(void)out_count;
			return Read_result::READ_ERR_INVALID;
		}

		bool directory(char const *path) override
		{
			/* empty path is this directory */
			return (!*path);
		}

		virtual Read_result read(Lwip_file_handle&,
		                         char *dst, file_size count,
		                         file_size &out_count) = 0;

		virtual Write_result write(Lwip_file_handle&,
		                           char const *src, file_size count,
		                           file_size &out_count) = 0;

		virtual bool read_ready(Lwip_file_handle&) = 0;

		void handle_io(int mask)
		{
			for (Lwip::Lwip_file_handle *h = handles.first();
			     h; h = h->next())
			{
				if (h->kind & mask) {
					io_handler.handle_io_response(h->context);
				}
			}
		}

		virtual Sync_result complete_sync() = 0;
};


Lwip::Lwip_file_handle::Lwip_file_handle(Vfs::File_system &fs, Allocator &alloc,
                                         int status_flags,
                                         Socket_dir &s, Lwip_file_handle::Kind k)
: Lwip_handle(fs, alloc, status_flags), socket(&s), kind(k)
{
	socket->handles.insert(this);
}

Lwip::Lwip_file_handle::~Lwip_file_handle()
{
	if (socket)
		socket->handles.remove(this);
}

Lwip::Read_result Lwip::Lwip_file_handle::read(char *dst, file_size count,
                                               file_size &out_count)
{
	return (socket)
		? socket->read(*this, dst, count, out_count)
		: Read_result::READ_ERR_INVALID;
}

Lwip::Write_result Lwip::Lwip_file_handle::write(char const *src, file_size count,
	                                             file_size &out_count)
{
	return (socket)
		? socket->write(*this, src, count, out_count)
		: Write_result::WRITE_ERR_INVALID;
}


void Lwip::Lwip_file_handle::print(Genode::Output &output) const
{
	output.out_string(socket->name().string());
	switch (kind) {

	case Lwip_file_handle::ACCEPT:   output.out_string("/accept"); break;
	case Lwip_file_handle::BIND:     output.out_string("/bind"); break;
	case Lwip_file_handle::CONNECT:  output.out_string("/connect"); break;
	case Lwip_file_handle::DATA:     output.out_string("/data"); break;
	case Lwip_file_handle::INVALID:  output.out_string("/invalid"); break;
	case Lwip_file_handle::LISTEN:   output.out_string("/listen"); break;
	case Lwip_file_handle::LOCAL:    output.out_string("/local"); break;
	case Lwip_file_handle::LOCATION: output.out_string("(location)"); break;
	case Lwip_file_handle::PENDING:  output.out_string("/accept_socket"); break;
	case Lwip_file_handle::PEEK:     output.out_string("/peek"); break;
	case Lwip_file_handle::REMOTE:   output.out_string("/remote"); break;
}
}


struct Lwip::Protocol_dir : Lwip::Directory
{
	virtual bool leaf_path(char const *) = 0;

	virtual Directory_service::Stat_result stat(char const*, Directory_service::Stat&) = 0;

	virtual void adopt_socket(Socket_dir &socket) = 0;

	virtual Open_result open(Vfs::File_system &fs,
	                         char const  *path,
	                         unsigned     mode,
	                         Vfs_handle **out_handle,
	                         Allocator   &alloc) = 0;

	virtual Opendir_result opendir(Vfs::File_system &fs,
	                               char const  *path,
	                               Vfs_handle **out_handle,
	                               Allocator   &alloc) = 0;

	virtual ~Protocol_dir() { }
};


template <typename SOCKET_DIR, typename PCB>
class Lwip::Protocol_dir_impl final : public Protocol_dir
{
	private:

		Genode::Allocator        &_alloc;
		Vfs::Io_response_handler &_io_handler;
		Genode::Entrypoint       &_ep;

		Genode::List<SOCKET_DIR> _socket_dirs { };

	public:

		friend class Genode::List<SOCKET_DIR>;
		friend class Genode::List<SOCKET_DIR>::Element;

		friend class Tcp_socket_dir;
		friend class Udp_socket_dir;

		Protocol_dir_impl(Vfs::Env &vfs_env)
		: _alloc(vfs_env.alloc()), _io_handler(vfs_env.io_handler()), _ep(vfs_env.env().ep()) { }

		SOCKET_DIR *lookup(char const *name)
		{
			if (*name == '/') ++name;

			/* make sure it is only a name */
			for (char const *p = name; *p; ++p)
				if (*p == '/')
					return nullptr;

			for (SOCKET_DIR *sd = _socket_dirs.first(); sd; sd = sd->next())
				if (*sd == name)
					return sd;

			return nullptr;
		}

		bool leaf_path(char const *path) override
		{
			Path subpath(path);
			subpath.strip_last_element();
			if ((subpath == "/") || (subpath == "/new_socket"))
				return true;

			if (lookup(subpath.string())) {
				subpath.import(path);
				subpath.keep_only_last_element();
				auto kind = Lwip_file_handle::kind_from_name(subpath);
				return (kind != Lwip_file_handle::INVALID);
			}
			return false;
		}

		Directory_service::Stat_result stat(char const *path, Directory_service::Stat &st) override
		{
			Path subpath(path);

			if (subpath == "/") {
				st.size = 1;
				st.mode = Directory_service::STAT_MODE_DIRECTORY;
				st.inode = (Genode::addr_t)this;
				return Directory_service::STAT_OK;
			}

			if (subpath == "/new_socket") {
				st.size = 1;
				st.mode = Directory_service::STAT_MODE_FILE | 0777;
				st.inode = ((Genode::addr_t)this)+1;
				return Directory_service::STAT_OK;
			}

			if (!subpath.has_single_element())
				subpath.strip_last_element();
			if (SOCKET_DIR *dir = lookup(subpath.string())) {
				Path filename(path);
				filename.keep_only_last_element();
				if (filename == subpath.base()) {
					st.size = Lwip_file_handle::INVALID;
					st.mode = Directory_service::STAT_MODE_DIRECTORY;
					st.inode = (Genode::addr_t)dir;
					return Directory_service::STAT_OK;
				}

				Lwip_file_handle::Kind k = Lwip_file_handle::kind_from_name(filename);
				if (k != Lwip_file_handle::INVALID) {
					st.mode = Directory_service::STAT_MODE_CHARDEV;
					st.inode = ((Genode::addr_t)dir)+k;
					return Directory_service::STAT_OK;
				}
			}
			return Directory_service::STAT_ERR_NO_ENTRY;
		}

		Read_result readdir(char *dst, file_size count,
		                    file_size &out_count) override
		{
			Genode::warning(__func__, " NOT_IMPLEMENTED");
			(void)dst;
			(void)count;
			(void)out_count;
			return Read_result::READ_ERR_INVALID;
		};

		bool directory(char const *path) override
		{
			/* empty path is the protocol directory */
			return *path ? (lookup(path+1) != nullptr) : true;
		}

		SOCKET_DIR &alloc_socket(Genode::Allocator &alloc, PCB *pcb = nullptr)
		{
			/*
			 * use the equidistribution RNG to hide the socket count,
			 * see src/lib/lwip/platform/rand.cc
			 */
			unsigned id = LWIP_RAND();

			/* check for collisions */
			for (SOCKET_DIR *dir = _socket_dirs.first(); dir;) {
				if (*dir == id) {
					id = LWIP_RAND();
					dir = _socket_dirs.first();
				} else {
					dir = dir->next();
				}
			}

			SOCKET_DIR *new_socket = new (alloc)
				SOCKET_DIR(id, *this, alloc, _io_handler, _ep, pcb);
			_socket_dirs.insert(new_socket);
			return *new_socket;
		}

		void adopt_socket(Socket_dir &socket) override {
			_socket_dirs.insert(static_cast<SOCKET_DIR*>(&socket)); }

		void release(SOCKET_DIR *socket) {
			_socket_dirs.remove(socket); }

		Open_result open(Vfs::File_system &fs,
		                 char const  *path,
		                 unsigned     mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			Path subpath(path);

			if (subpath == "/new_socket") {
				Socket_dir &new_dir = alloc_socket(alloc);
				*out_handle = new (alloc) Lwip_file_handle(
					fs, alloc, Vfs::Directory_service::OPEN_MODE_RDONLY,
					new_dir, Lwip_file_handle::LOCATION);
				return Open_result::OPEN_OK;
			}

			subpath.strip_last_element();
			if (SOCKET_DIR *dir = lookup(subpath.base()+1)) {
				subpath.import(path);
				subpath.keep_only_last_element();
				return dir->open(fs, subpath, mode, out_handle, alloc);
			}

			return Open_result::OPEN_ERR_UNACCESSIBLE;
		}

		Opendir_result opendir(Vfs::File_system &fs,
		                       char const  *path,
		                       Vfs_handle **out_handle,
		                       Allocator   &alloc) override
		{
			if (!*path) {
				*out_handle = new (alloc) Lwip_dir_handle(fs, alloc, *this);
				return Opendir_result::OPENDIR_OK;
			}
			if (SOCKET_DIR *dir = lookup(path)) {
				*out_handle = new (alloc) Lwip_dir_handle(fs, alloc, *dir);
				return Opendir_result::OPENDIR_OK;
			}

			return Opendir_result::OPENDIR_ERR_LOOKUP_FAILED;
		}

		void notify()
		{
			for (SOCKET_DIR *sd = _socket_dirs.first(); sd; sd = sd->next()) {
				sd->handle_io(~0U);
			}
		}
};

namespace Lwip {
	typedef Protocol_dir_impl<Udp_socket_dir, udp_pcb> Udp_proto_dir;
	typedef Protocol_dir_impl<Tcp_socket_dir, tcp_pcb> Tcp_proto_dir;
}


/*********
 ** UDP **
 *********/

class Lwip::Udp_socket_dir final :
	public Socket_dir,
	private Udp_socket_dir_list::Element
{
	private:

		/*
		  * Noncopyable
		 */
		Udp_socket_dir(Udp_socket_dir const &);
		Udp_socket_dir &operator = (Udp_socket_dir const &);

		Udp_proto_dir &_proto_dir;

		udp_pcb *_pcb;

		/**
		 * Handle on a received UDP packet
		 */
		class Packet : public Genode::Fifo<Packet>::Element
		{
			public:

				Packet(Packet const &);
				Packet &operator = (Packet const &);

				ip_addr_t const addr;
				u16_t     const port;

			private:

				u16_t  offset = 0;
				pbuf  *buf;

			public:

				Packet(ip_addr_t const *addr, u16_t port, pbuf *buf)
				: addr(*addr), port(port), buf(buf) { }

				~Packet() { pbuf_free(buf); }

				u16_t read(void *dst, size_t count)
				{
					count = min((size_t)buf->tot_len, count);
					auto n = pbuf_copy_partial(buf, dst, count, offset);
					offset += n;
					return n;
				}

				u16_t peek(void *dst, size_t count)
				{
					count = min((size_t)buf->tot_len, count);
					return pbuf_copy_partial(buf, dst, count, offset);
				}

				bool empty() { return offset >= buf->tot_len; }
		};

		Genode::Tslab<Packet, sizeof(Packet)*64> _packet_slab { &alloc };

		/* Queue of received UDP packets */
		Genode::Fifo<Packet> _packet_queue { };

		/* destination addressing */
		ip_addr_t _to_addr { };
		u16_t     _to_port = 0;

		/**
		 * New sockets from accept not avaiable for UDP
		 */
		Open_result _accept_new_socket(Vfs::File_system &,
                                       Genode::Allocator &,
                                       Vfs::Vfs_handle **) override {
			return Open_result::OPEN_ERR_UNACCESSIBLE; }

	public:

		friend class Udp_socket_dir_list;
		friend class Udp_socket_dir_list::Element;
		using Udp_socket_dir_list::Element::next;

		Udp_socket_dir(unsigned num, Udp_proto_dir &proto_dir,
		               Genode::Allocator &alloc,
		               Vfs::Io_response_handler &io_handler,
		               Genode::Entrypoint &,
		               udp_pcb *pcb)
		: Socket_dir(num, alloc, io_handler),
		  _proto_dir(proto_dir), _pcb(pcb ? pcb : udp_new())
		{
			ip_addr_set_zero(&_to_addr);

			/* 'this' will be the argument to the LwIP recv callback */
			udp_recv(_pcb, udp_recv_callback, this);
		}

		virtual ~Udp_socket_dir()
		{
			udp_remove(_pcb);
			_pcb = NULL;

			_proto_dir.release(this);
		}

		/**
		 * Stuff a packet in the queue and notify the handle
		 */
		void queue(ip_addr_t const *addr, u16_t port, pbuf *buf)
		{
			try {
				Packet *pkt = new (_packet_slab) Packet(addr, port, buf);
				_packet_queue.enqueue(pkt);
			} catch (...) {
				Genode::warning("failed to queue UDP packet, dropping");
				pbuf_free(buf);
			}

			handle_io(Lwip_file_handle::REMOTE|Lwip_file_handle::DATA_READY);
		}


		/**************************
		 ** Socket_dir interface **
		 **************************/

		bool read_ready(Lwip_file_handle &handle) override
		{
			switch (handle.kind) {
			case Lwip_file_handle::DATA:
			case Lwip_file_handle::REMOTE:
			case Lwip_file_handle::PEEK:
				return !_packet_queue.empty();
			default:
				break;
			}
			return true;
		}

		Read_result read(Lwip_file_handle &handle,
		                 char *dst, file_size count,
		                 file_size &out_count) override
		{
			switch(handle.kind) {

			case Lwip_file_handle::DATA: {
				if (Packet *pkt = _packet_queue.head()) {
					out_count = pkt->read(dst, count);
					if (pkt->empty()) {
						destroy(_packet_slab, _packet_queue.dequeue());
					}
					return Read_result::READ_OK;
				}
				return Read_result::READ_QUEUED;
			}

			case Lwip_file_handle::PEEK: {
				if (Packet *pkt = _packet_queue.head()) {
					out_count = pkt->peek(dst, count);
				}
				return Read_result::READ_OK;
			}

			case Lwip_file_handle::LOCAL:
			case Lwip_file_handle::BIND: {
				if (count < ENDPOINT_STRLEN_MAX)
					return Read_result::READ_ERR_INVALID;
				char const *ip_str = ipaddr_ntoa(&_pcb->local_ip);
				/* TODO: [IPv6]:port */
				out_count = Genode::snprintf(dst, count, "%s:%d\n",
				                             ip_str, _pcb->local_port);
				return Read_result::READ_OK;
			}

			case Lwip_file_handle::CONNECT: {
				/* check if the PCB was connected */
				if (ip_addr_isany(&_pcb->remote_ip))
					return Read_result::READ_OK;
				/* otherwise fallthru to REMOTE*/
			}

			case Lwip_file_handle::REMOTE: {
				if (count < ENDPOINT_STRLEN_MAX) {
					Genode::error("VFS LwIP: accept file read buffer is too small");
					return Read_result::READ_ERR_INVALID;
				}
				if (ip_addr_isany(&_pcb->remote_ip)) {
					if (Packet *pkt = _packet_queue.head()) {
						char const *ip_str = ipaddr_ntoa(&pkt->addr);
						/* TODO: IPv6 */
						out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                                 ip_str, pkt->port);
						return Read_result::READ_OK;
					}
				} else {
					char const *ip_str = ipaddr_ntoa(&_pcb->remote_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                             ip_str, _pcb->remote_port);
					return Read_result::READ_OK;
				}
				break;
			}

			case Lwip_file_handle::LOCATION:
				/*
				 * Print the location of this socket directory
				 */
				out_count = Genode::snprintf(
					dst, count, "udp/%s\n", name().string());
				return Read_result::READ_OK;
				break;

			default: break;
			}

			return Read_result::READ_ERR_INVALID;
		}

		Write_result write(Lwip_file_handle &handle,
		                   char const *src, file_size count,
		                   file_size &out_count) override
		{
			switch(handle.kind) {

			case Lwip_file_handle::DATA: {
				if (ip_addr_isany(&_to_addr)) break;

				file_size remain = count;
				while (remain) {
					pbuf *buf = pbuf_alloc(PBUF_RAW, remain, PBUF_RAM);
					pbuf_take(buf, src, buf->tot_len);

					char shit[ENDPOINT_STRLEN_MAX];
					ipaddr_aton(shit, &_to_addr);
					err_t err = udp_sendto(_pcb, buf, &_to_addr, _to_port);
					pbuf_free(buf);
					if (err != ERR_OK)
						return Write_result::WRITE_ERR_IO;
					remain -= buf->tot_len;
					src += buf->tot_len;
				}
				out_count = count;
				return Write_result::WRITE_OK;
			}

			case Lwip_file_handle::REMOTE: {
				if (!ip_addr_isany(&_pcb->remote_ip)) {
					return Write_result::WRITE_ERR_INVALID;
				} else {
					char buf[ENDPOINT_STRLEN_MAX];
					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					_to_port = remove_port(buf);
					out_count = count;
					if (ipaddr_aton(buf, &_to_addr)) {
						out_count = count;
						return Write_result::WRITE_OK;
					}
				}
				break;
			}

			case Lwip_file_handle::BIND: {
				if (count < ENDPOINT_STRLEN_MAX) {
					char buf[ENDPOINT_STRLEN_MAX];
					ip_addr_t addr;
					u16_t port;

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));
					port = remove_port(buf);
					if (!ipaddr_aton(buf, &addr))
						break;

					err_t err = udp_bind(_pcb, &addr, port);
					if (err == ERR_OK) {
						out_count = count;
						return Write_result::WRITE_OK;
					}
					return Write_result::WRITE_ERR_IO;
				}
				break;
			}

			case Lwip_file_handle::CONNECT: {
				if (count < ENDPOINT_STRLEN_MAX) {
					char buf[ENDPOINT_STRLEN_MAX];

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					_to_port = remove_port(buf);
					if (!ipaddr_aton(buf, &_to_addr))
						break;

					err_t err = udp_connect(_pcb, &_to_addr, _to_port);
					if (err != ERR_OK) {
						Genode::error("lwIP: failed to connect UDP socket, error ", (int)-err);
						return Write_result::WRITE_ERR_IO;
					}

					out_count = count;
					return Write_result::WRITE_OK;
				}
				break;
			}

			default: break;
			}

			return Write_result::WRITE_ERR_INVALID;
		}

		Sync_result complete_sync() override { return Sync_result::SYNC_OK; };
};


/*********
 ** TCP **
 *********/

class Lwip::Tcp_socket_dir final :
	public  Socket_dir,
	private Tcp_socket_dir_list::Element
{
	public:

		struct Pcb_pending : Genode::List<Pcb_pending>::Element
		{
			tcp_pcb *pcb;
			pbuf *buf = nullptr;

			Pcb_pending(tcp_pcb *p) : pcb(p) { }
		};

	private:

		/*
		  * Noncopyable
		 */
		Tcp_socket_dir(Tcp_socket_dir const &);
		Tcp_socket_dir &operator = (Tcp_socket_dir const &);

		Tcp_proto_dir       &_proto_dir;
		Genode::Entrypoint  &_ep;

		typedef Genode::List<Pcb_pending> Pcb_pending_list;

		Pcb_pending_list     _pcb_pending { };
		tcp_pcb             *_pcb;

		/* queue of received data */
		pbuf *_recv_pbuf = nullptr;
		u16_t _recv_off  = 0;

		Open_result _accept_new_socket(Vfs::File_system &fs,
                                       Genode::Allocator &alloc,
                                       Vfs::Vfs_handle **out_handle) override
		{
			*out_handle = new (alloc) Lwip_file_handle(
				fs, alloc, Vfs::Directory_service::OPEN_MODE_RDONLY,
				*this, Lwip_file_handle::PENDING);
			return Open_result::OPEN_OK;
		}

	public:

		friend class Tcp_socket_dir_list;
		friend class Tcp_socket_dir_list::Element;
		using Tcp_socket_dir_list::Element::next;

		State state;

		Tcp_socket_dir(unsigned num, Tcp_proto_dir &proto_dir,
		               Genode::Allocator &alloc,
		               Vfs::Io_response_handler &io_handler,
		               Genode::Entrypoint &ep,
		               tcp_pcb *pcb)
		: Socket_dir(num, alloc, io_handler), _proto_dir(proto_dir),
		  _ep(ep), _pcb(pcb ? pcb : tcp_new()), state(pcb ? READY : NEW)
		{
			/* 'this' will be the argument to LwIP callbacks */
			tcp_arg(_pcb, this);

			tcp_recv(_pcb, tcp_recv_callback);

			/* Disabled, do not track acknowledgements */
			/* tcp_sent(_pcb, tcp_sent_callback); */

			tcp_err(_pcb, tcp_err_callback);
		}

		~Tcp_socket_dir()
		{
			tcp_arg(_pcb, NULL);

			for (Pcb_pending *p = _pcb_pending.first(); p; p->next()) {
				destroy(alloc, p);
			}

			if (_pcb != NULL) {
				tcp_arg(_pcb, NULL);
				tcp_close(_pcb);
			}

			_proto_dir.release(this);
		}

		/**
		 * Accept new connection from callback
		 */
		err_t accept(struct tcp_pcb *newpcb, err_t)
		{
			Pcb_pending *elem = new (alloc) Pcb_pending(newpcb);
			_pcb_pending.insert(elem);

			tcp_backlog_delayed(newpcb);

			tcp_arg(newpcb, elem);
			tcp_recv(newpcb, tcp_delayed_recv_callback);

			handle_io(Lwip_file_handle::ACCEPT|Lwip_file_handle::PENDING);
			return ERR_OK;
		}

		/**
		 * chain a buffer to the queue
		 */
		void recv(struct pbuf *buf)
		{
			if (_recv_pbuf && buf) {
				pbuf_cat(_recv_pbuf, buf);
			} else {
				_recv_pbuf = buf;
			}
		}

		/**
		 * Close the connection by error
		 *
		 * Triggered by error callback, usually
		 * just by an aborted connection.
		 */
		void error()
		{
			state = CLOSED;

			/* the PCB is expired now */
			if (_pcb) {
				tcp_arg(_pcb, NULL);
				tcp_close(_pcb);
				_pcb = NULL;
			}

			/* churn the application */
			handle_io(~0U);
		}

		/**
		 * Close the connection
		 *
		 * Can be triggered by remote shutdown via callback
		 */
		void shutdown()
		{
			state = CLOSING;
			if (_recv_pbuf)
				return;

			if (_pcb) {
				tcp_arg(_pcb, NULL);
				tcp_close(_pcb);
				state = CLOSED;
				_pcb = NULL;
			}
		}

		/**************************
		 ** Socket_dir interface **
		 **************************/

		bool read_ready(Lwip_file_handle &handle) override
		{
			switch (handle.kind) {
			case Lwip_file_handle::DATA:
			case Lwip_file_handle::PEEK:
				switch (state) {
				case READY:
					return _recv_pbuf != NULL;
				case CLOSING:
				case CLOSED:
					/* time for the application to find out */
					return true;
				default: break;
				}
				break;

			case Lwip_file_handle::ACCEPT:
			case Lwip_file_handle::PENDING:
				return _pcb_pending.first() != nullptr;

			case Lwip_file_handle::BIND:
				return state != NEW;

			case Lwip_file_handle::REMOTE:
				switch (state) {
				case NEW:
				case BOUND:
				case LISTEN:
					break;
				default:
					return true;
				}
				break;

			case Lwip_file_handle::CONNECT:
				return !ip_addr_isany(&_pcb->remote_ip);

			case Lwip_file_handle::LOCATION:
			case Lwip_file_handle::LOCAL:
				return true;
			default: break;
			}

			return false;
		}

		Read_result read(Lwip_file_handle &handle,
		                 char *dst, file_size count,
		                 file_size &out_count) override
		{
			switch(handle.kind) {

			case Lwip_file_handle::DATA:
				{
					if (_recv_pbuf == nullptr) {
						/*
						 * queue the read if the PCB is active and
						 * there is nothing to read, otherwise return
						 * zero to indicate the connection is closed
						 */
						return (state == READY)
							? Read_result::READ_QUEUED
							: Read_result::READ_OK;
					}

					u16_t const ucount = count;
					u16_t const n = pbuf_copy_partial(_recv_pbuf, dst, ucount, _recv_off);
					_recv_off += n;
					{
						u16_t new_off;
						pbuf *new_head = pbuf_skip(_recv_pbuf, _recv_off, &new_off);
						if (new_head != NULL && new_head != _recv_pbuf) {
							/* move down the buffer and deref the head */
							pbuf_ref(new_head);
							pbuf_realloc(new_head, _recv_pbuf->tot_len+_recv_off);
							pbuf_free(_recv_pbuf);
						}

						if (!new_head)
							pbuf_free(_recv_pbuf);
						_recv_pbuf = new_head;
						_recv_off = new_off;
					}

					/* ACK the remote */
					if (_pcb)
						tcp_recved(_pcb, n);

					if (state == CLOSING)
						shutdown();

					out_count = n;
					return Read_result::READ_OK;
				}
				break;

			case Lwip_file_handle::PEEK:
				if (_recv_pbuf != nullptr) {
					u16_t const ucount = count;
					u16_t const n = pbuf_copy_partial(_recv_pbuf, dst, ucount, _recv_off);
					out_count = n;
				}
				return Read_result::READ_OK;

			case Lwip_file_handle::REMOTE:
				if (state == READY) {
					if (count < ENDPOINT_STRLEN_MAX)
						return Read_result::READ_ERR_INVALID;
					char const *ip_str = ipaddr_ntoa(&_pcb->remote_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                             ip_str, _pcb->remote_port);
					return Read_result::READ_OK;
				} else if (state == CLOSED) {
					return Read_result::READ_OK;
				}
				break;

			case Lwip_file_handle::PENDING: {
				if (Pcb_pending *pp = _pcb_pending.first()) {
					Tcp_socket_dir &new_dir = _proto_dir.alloc_socket(alloc, pp->pcb);
					new_dir._recv_pbuf = pp->buf;

					handles.remove(&handle);
					handle.socket = &new_dir;
					new_dir.handles.insert(&handle);

					tcp_backlog_accepted(pp->pcb);

					_pcb_pending.remove(pp);
					destroy(alloc, pp);

					handle.kind = Lwip_file_handle::LOCATION;
					/* read the location of the new socket directory */
					return handle.read(dst, count, out_count);
				}

				return Read_result::READ_QUEUED;
			}

			case Lwip_file_handle::LOCATION:
				/*
				 * Print the location of this socket directory
				 */
				out_count = Genode::snprintf(
					dst, count, "tcp/%s\n", name().string());
				return Read_result::READ_OK;
				break;

			case Lwip_file_handle::ACCEPT: {
				/*
				 * Print the number of pending connections
				 */
				int pending_count = 0;
				for (Pcb_pending *p = _pcb_pending.first(); p; p = p->next())
					++pending_count;

				out_count = Genode::snprintf(
					dst, count, "%d\n", pending_count);
				return Read_result::READ_OK;
			}

			case Lwip_file_handle::LOCAL:
			case Lwip_file_handle::BIND:
				if (state != CLOSED) {
					if (count < ENDPOINT_STRLEN_MAX)
						return Read_result::READ_ERR_INVALID;
					char const *ip_str = ipaddr_ntoa(&_pcb->local_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(
						dst, count, "%s:%d\n", ip_str, _pcb->local_port);
					return Read_result::READ_OK;
				}
				break;

			case Lwip_file_handle::CONNECT:
			case Lwip_file_handle::LISTEN:
			case Lwip_file_handle::INVALID: break;
			}

			return Read_result::READ_ERR_INVALID;
		}

		Write_result write(Lwip_file_handle &handle,
		                   char const *src, file_size count,
		                   file_size &out_count) override
		{
			if (_pcb == NULL) {
				/* socket is closed */
				return Write_result::WRITE_ERR_INVALID;
			}

			switch(handle.kind) {
			case Lwip_file_handle::DATA:
				if (state == READY) {
					file_size out = 0;
					while (count) {
						/* check if the send buffer is exhausted */
						if (tcp_sndbuf(_pcb) == 0) {
							Genode::warning("TCP send buffer congested");
							out_count = out;
							return out
								? Write_result::WRITE_OK
								: Write_result::WRITE_ERR_WOULD_BLOCK;
						}

						u16_t n = min(count, tcp_sndbuf(_pcb));
						/* how much can we queue right now? */

						count -= n;
						/* write to outgoing TCP buffer */
						err_t err = tcp_write(
							_pcb, src, n, TCP_WRITE_FLAG_COPY);
						if (err == ERR_OK)
							/* flush the buffer */
							err = tcp_output(_pcb);
						if (err != ERR_OK) {
							Genode::error("lwIP: tcp_write failed, error ", (int)-err);
							return Write_result::WRITE_ERR_IO;
						}

						src += n;
						out += n;
						/* pending_ack += n; */
					}

					out_count = out;
					return Write_result::WRITE_OK;
				}
				break;

			case Lwip_file_handle::BIND:
				if ((state == NEW) && (count < ENDPOINT_STRLEN_MAX)) {
					char buf[ENDPOINT_STRLEN_MAX];
					ip_addr_t addr;
					u16_t port = 0;

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					port = remove_port(buf);
					if (!ipaddr_aton(buf, &addr))
						break;

					err_t err = tcp_bind(_pcb, &addr, port);
					if (err == ERR_OK) {
						state = BOUND;
						out_count = count;
						return Write_result::WRITE_OK;
					}
				}
				break;

			case Lwip_file_handle::CONNECT:
				if (((state == NEW) || (state == BOUND)) && (count < ENDPOINT_STRLEN_MAX-1)) {
					char buf[ENDPOINT_STRLEN_MAX];
					ip_addr_t addr;
					u16_t port = 0;

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));
					port = remove_port(buf);
					if (!ipaddr_aton(buf, &addr))
						break;

					err_t err = tcp_connect(_pcb, &addr, port, tcp_connect_callback);
					if (err != ERR_OK) {
						Genode::error("lwIP: failed to connect TCP socket, error ", (int)-err);
						return Write_result::WRITE_ERR_IO;
					}
					state = CONNECT;
					out_count = count;
					return Write_result::WRITE_OK;
				}
				break;

			case Lwip_file_handle::LISTEN:
				if ((state == BOUND) && (count < 7)) {
					unsigned long backlog = TCP_DEFAULT_LISTEN_BACKLOG;
					char buf[8];

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));
					Genode::ascii_to_unsigned(buf, backlog, 10);

					/* this replaces the PCB so set the callbacks again */
					_pcb = tcp_listen_with_backlog(_pcb, backlog);
					tcp_arg(_pcb, this);
					tcp_accept(_pcb, tcp_accept_callback);
					state = LISTEN;
					out_count = count;
					return Write_result::WRITE_OK;
				}
				break;

			default: break;
			}

			return Write_result::WRITE_ERR_INVALID;
		}

		Sync_result complete_sync() override
		{
			switch (state) {
			case CONNECT:
				/* sync will queue until the socket is connected and ready */
				return Sync_result::SYNC_QUEUED;
			case CLOSED:
				/* assumed to be caused by error */
				return Sync_result::SYNC_ERR_INVALID;
			default:
				return Sync_result::SYNC_OK;
			}
		}
};


/********************
 ** LwIP callbacks **
 ********************/

namespace Lwip {
	extern "C" {

static
void udp_recv_callback(void *arg, struct udp_pcb*, struct pbuf *buf, const ip_addr_t *addr, u16_t port)
{
	if (arg) {
		Lwip::Udp_socket_dir *socket_dir =
			static_cast<Lwip::Udp_socket_dir *>(arg);
		socket_dir->queue(addr, port, buf);
	} else {
		pbuf_free(buf);
	}
}


static
err_t tcp_connect_callback(void *arg, struct tcp_pcb *pcb, err_t)
{
	if (!arg) {
		tcp_abort(pcb);
		return ERR_ABRT;
	}

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	socket_dir->state = Lwip::Tcp_socket_dir::READY;

	socket_dir->handle_io(
		Lwip_file_handle::CONNECT |
		Lwip_file_handle::DATA_READY);
	return ERR_OK;
}


static
err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	if (!arg) {
		tcp_abort(newpcb);
		return ERR_ABRT;
	}

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	err_t e = socket_dir->accept(newpcb, err);
	return e;
};


static
err_t tcp_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t)
{
	if (!arg) {
		tcp_abort(pcb);
		return ERR_ABRT;
	}

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	if (p == NULL) {
		socket_dir->shutdown();
	} else {
		socket_dir->recv(p);
	}
	socket_dir->handle_io(Lwip_file_handle::DATA_READY);
	return ERR_OK;
}


static
err_t tcp_delayed_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *buf, err_t)
{
	if (!arg) {
		tcp_abort(pcb);
		return ERR_ABRT;
	}

	Lwip::Tcp_socket_dir::Pcb_pending *pending =
		static_cast<Lwip::Tcp_socket_dir::Pcb_pending *>(arg);

	if (pending->buf && buf) {
		pbuf_cat(pending->buf, buf);
	} else {
		pending->buf = buf;
	}

	return ERR_OK;
};


/**
 * This would be the ACK callback, we could defer sync completion
 * until then, but performance is expected to be unacceptable.
 *
static
err_t tcp_sent_callback(void *arg, struct tcp_pcb*, u16_t len)
{
	if (!arg) {
		tcp_abort(pcb);
		return ERR_ABRT;
	}

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	socket_dir->pending_ack -= len;
	socket_dir->handle_io(Lwip_file_handle::DATA);
	return ERR_OK;
}
*/


static
void tcp_err_callback(void *arg, err_t)
{
	if (!arg) return;

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	socket_dir->error();
	/* the error is ERR_ABRT or ERR_RST, both end the session */
}

	}
}


/*********************
 ** VFS file-system **
 *********************/

class Lwip::File_system final : public Vfs::File_system
{
	private:

		/**
		 * LwIP connection to Nic service
		 */
		struct Vfs_netif : Lwip::Nic_netif
		{
			Vfs::Io_response_handler &io_handler;

			Tcp_proto_dir tcp_dir;
			Udp_proto_dir udp_dir;

			Nameserver_registry nameserver_handles { };

			/**
			 * Wake the application when the interface changes.
			 */
			void status_callback() override
			{
				tcp_dir.notify();
				udp_dir.notify();

				nameserver_handles.for_each([&] (Lwip_nameserver_handle &h) {
					io_handler.handle_io_response(h.context); });
			}

			Vfs_netif(Vfs::Env &vfs_env,
			          Genode::Xml_node config,
			          Vfs::Io_response_handler &io)
			: Lwip::Nic_netif(vfs_env.env(), vfs_env.alloc(), config),
			  io_handler(io), tcp_dir(vfs_env), udp_dir(vfs_env)
			{ }
		} _netif;

		/**
		 * Walk a path to a protocol directory and apply procedure
		 */
		template <typename PROC>
		void apply_walk(char const *path, PROC const proc)
		{
			if (*path == '/') ++path;

			if (Genode::strcmp(path, "tcp", 3) == 0)
				proc(path+3, _netif.tcp_dir);
			else
			if (Genode::strcmp(path, "udp", 3) == 0)
				proc(path+3, _netif.udp_dir);
		}

		static bool match_nameserver(char const *name) {
			return (!strcmp(name, "nameserver")); }

	public:

		File_system(Vfs::Env &vfs_env, Genode::Xml_node config)
		: _netif(vfs_env, config, vfs_env.io_handler())
		{ }

		/**
		 * Reconfigure the LwIP Nic interface with the VFS config hook
		 */
		void apply_config(Genode::Xml_node const &node) override {
			_netif.configure(node); }


		/***********************
		 ** Directory_service **
		 ***********************/

		char const *leaf_path(char const *path) override
		{
			if (*path == '/') ++path;
			if (match_nameserver(path))
				return path;

			char const *r = nullptr;
			apply_walk(path, [&] (char const *subpath, Protocol_dir &dir) {
				if (dir.leaf_path(subpath)) r = path;
			});
			return r;
		}

		Stat_result stat(char const *path, Stat &st) override
		{
			if (*path == '/') ++path;
			st = Stat();
			st.device = (Genode::addr_t)this;

			if (match_nameserver(path)) {
				st.size = IPADDR_STRLEN_MAX;
				st.mode = STAT_MODE_FILE;
				return STAT_OK;
			}

			Stat_result r = STAT_ERR_NO_PERM;

			apply_walk(path, [&] (char const *subpath, Protocol_dir &dir) {
				r = dir.stat(subpath, st);
			});
			return r;
		}

		bool directory(char const *path) override
		{
			if (*path == '/') ++path;
			if (*path == '\0') return true;

			bool r = false;
			apply_walk(path, [&] (char const *subpath, Protocol_dir &dir) {
				r = dir.directory(subpath);
			});
			return r;
		}

		Open_result open(char const  *path,
		                 unsigned     mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (*path == '/') ++path;

			/**
			 * No files may be created here
			 */
			if (mode & OPEN_MODE_CREATE) return OPEN_ERR_NO_PERM;

			if (match_nameserver(path)) {
				*out_handle = new (alloc)
					Lwip_nameserver_handle(*this, alloc, _netif.nameserver_handles);
				return OPEN_OK;
			}

			Open_result r = OPEN_ERR_UNACCESSIBLE;
			apply_walk(path, [&] (char const *subpath, Protocol_dir &dir) {
				r = dir.open(*this, subpath, mode, out_handle, alloc);
			});
			return r;
		}

		Opendir_result opendir(char const  *path, bool create,
		                       Vfs_handle **out_handle,
		                       Allocator   &alloc) override
		{
			/**
			 * No directories may be created here
			 */
			if (create) return OPENDIR_ERR_PERMISSION_DENIED;

			Opendir_result r = OPENDIR_ERR_LOOKUP_FAILED;
			apply_walk(path, [&] (char const *subpath, Protocol_dir &dir) {
				r = dir.opendir(*this, subpath, out_handle, alloc);
			});
			return r;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			Socket_dir *socket = nullptr;
			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle)) {
				if (Lwip_file_handle *file_handle = dynamic_cast<Lwip_file_handle*>(handle)) {
					socket = file_handle->socket;
				}
				destroy(handle->alloc(), handle);
			} else {
				Genode::error("refusing to destroy strange handle");
			}

			/* destroy sockets that are not referenced by any handles */
			if (socket && !socket->handles.first()) {
				destroy(socket->alloc, socket);
			}
		}

		Unlink_result unlink(char const *) override {
			return UNLINK_ERR_NO_PERM; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *src, file_size count,
		                   file_size &out_count) override
		{
			out_count = 0;

			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_RDONLY)
				return Write_result::WRITE_ERR_INVALID;
			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle))
				return handle->write(src, count, out_count);
			return Write_result::WRITE_ERR_INVALID;
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                                  char *dst, file_size count,
		                                  file_size &out_count) override
		{
			/*
			 * LwIP buffer operations are limited to sizes that
			 * can be expressed in sixteen bits
			 */
			count = Genode::min(count, 0xffffU);

			out_count = 0;

			if ((!vfs_handle) ||
			    ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_WRONLY)) {
				return Read_result::READ_ERR_INVALID;
			}

			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle))
				return handle->read(dst, count, out_count);
			return Read_result::READ_ERR_INVALID;
		}

		bool queue_read(Vfs_handle *, file_size) {
			return _netif.ready(); }

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			if (Lwip_file_handle *handle = dynamic_cast<Lwip_file_handle*>(vfs_handle)) {
				if (handle->socket)
					return handle->socket->read_ready(*handle);
			}

			/*
			 * in this case the polled file is a 'new_socket'
			 * or a file with no associated socket
			 */
			return true;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			if (Lwip_file_handle *handle = dynamic_cast<Lwip_file_handle*>(vfs_handle)) {
				if (handle->socket) {
					return true;
				}
			}
			return false;
		}

		bool check_unblock(Vfs_handle*, bool, bool, bool)
		{
			Genode::error("VFS lwIP: ",__func__," not implemented");
			return true;
		}

		Sync_result complete_sync(Vfs_handle *vfs_handle) override
		{
			Lwip_file_handle *h = dynamic_cast<Lwip_file_handle*>(vfs_handle);
			if (h) {
				if (h->socket) {
					return h->socket->complete_sync();
				} else {
					return SYNC_QUEUED;
				}
			}
			return SYNC_OK;
		}

		/***********************
		 ** File system stubs **
		 ***********************/

		Rename_result rename(char const *, char const *) override {
			return RENAME_ERR_NO_PERM; }

		file_size num_dirent(char const *) override {
			return 0; }

		Dataspace_capability dataspace(char const *) override {
			return Dataspace_capability(); }

		void release(char const *, Dataspace_capability) override { };

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			/* report ok because libc always executes ftruncate() when opening rw */
			return FTRUNCATE_OK;
		}

		char const *type() override { return "lwip"; }
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Genode::Constructible<Timer::Connection> timer { };

		Vfs::File_system *create(Vfs::Env &vfs_env, Genode::Xml_node config) override
		{
			if (!timer.constructed()) {
				timer.construct(vfs_env.env(), "vfs_lwip");
				Lwip::genode_init(vfs_env.alloc(), *timer);
			}

			return new (vfs_env.alloc()) Lwip::File_system(vfs_env, config);
		}
	};

	static Factory f;
	return &f;
}

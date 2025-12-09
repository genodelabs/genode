/*
 * \brief  Sockopt value/directory file-systems
 * \author Sebastian Sumpf
 * \date   2025-09-29
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#ifndef _SOCKOPT_H_
#define _SOCKOPT_H_

/* Genode includes */
#include <vfs/single_file_system.h>
#include <vfs/dir_file_system.h>

namespace Vfs_ip {

	using namespace Genode;
	using namespace Genode::Vfs;

	template <Sock_level, Sock_opt, bool READONLY = false>
	class Sockopt_value_file_system;
	class Sockopt_factory;
	class Sockopt_file_system;
}


template <Sock_level LEVEL, Sock_opt OPTNAME, bool READONLY>
class Vfs_ip::Sockopt_value_file_system : public Single_file_system
{
	public:

		using Name = Genode::String<64>;

	private:

		using Allocator            = Genode::Allocator;
		using Byte_range_ptr       = Genode::Byte_range_ptr;
		using Const_byte_range_ptr = Genode::Const_byte_range_ptr;

		using size_t = Genode::size_t;

		enum { BUF_SIZE = sizeof(long) };

		Name const _file_name;

		genode_socket_handle &_sock;

		struct Vfs_handle : Single_vfs_handle
		{
			genode_socket_handle &_sock;

			Vfs_handle(genode_socket_handle      &sock,
			           Sockopt_value_file_system &fs,
			           Allocator                &alloc)
			:
				Single_vfs_handle(fs, fs, alloc, 0),
				_sock(sock)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				out_count = 0;

				long     opt = 0;
				unsigned len = BUF_SIZE;
				Errno err = genode_socket_getsockopt(&_sock, LEVEL, OPTNAME, &opt, &len);
				if (err != GENODE_ENONE) return READ_ERR_IO;

				len = Genode::min(len, unsigned(dst.num_bytes));
				Genode::memcpy(dst.start, &opt, len);
				out_count = len;

				return READ_OK;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				out_count = 0;

				if (READONLY || src.num_bytes > BUF_SIZE) return WRITE_ERR_INVALID;

				long opt = 0;
				Genode::memcpy(&opt, src.start, src.num_bytes);

				Errno err = genode_socket_setsockopt(&_sock, LEVEL, OPTNAME, &opt, BUF_SIZE);
				if (err != GENODE_ENONE) return WRITE_ERR_IO;

				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return READONLY ? false : true; }

			private:

				Vfs_handle(Vfs_handle const &);
				Vfs_handle &operator = (Vfs_handle const &); 
		};

		using Config = Genode::String<200>;
		Config _config(Name const &name) const
		{
			char buf[Config::capacity()] { };
			Genode::Generator::generate({ buf, sizeof(buf) }, type_name(),
				[&] (Genode::Generator &g) { g.attribute("name", name); }
			).with_error([&] (Genode::Buffer_error) {
				Genode::warning("VFS value fs config failed (", _file_name, ")");
			});
			return Config(Genode::Cstring(buf));
		}

	public:

		Sockopt_value_file_system(Name const &name, genode_socket_handle &sock)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type(),
			                   Node_rwx::rw(), Node(_config(name))),
			_file_name(name), _sock(sock) { }

		static char const *type_name() { return "sockopt"; }

		char const *type() override { return type_name(); }

		bool matches(Genode::Node const &node) const
		{
			return node.has_type(type_name()) &&
			       node.attribute_value("name", Name()) == _file_name;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size size) override
		{
			if (size >= BUF_SIZE)
				return FTRUNCATE_ERR_NO_SPACE;

			return FTRUNCATE_OK;
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc) Vfs_handle(_sock, *this, alloc);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { Genode::error("out of ram"); return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { Genode::error("out of caps");return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = BUF_SIZE;
			return result;
		}

		using Single_file_system::close;
};


class Vfs_ip::Sockopt_factory : public File_system_factory
{
	private:

		template<Sock_opt OPTNAME, bool READONLY = false>
		using Sockopt = Sockopt_value_file_system<GENODE_SOL_SOCKET, OPTNAME, READONLY>;

		template<Sock_opt OPTNAME>
		using Readonly_sockopt = Sockopt<OPTNAME, true>;

		template<Sock_opt OPTNAME>
		using Tcpopt = Sockopt_value_file_system<GENODE_IPPROTO_TCP, OPTNAME>;

		genode_socket_handle &_sock;

		Readonly_sockopt<GENODE_SO_ERROR> _so_error { "so_error", _sock };

		Sockopt<GENODE_SO_KEEPALIVE> _so_keepalive  { "so_keepalive", _sock };
		Sockopt<GENODE_SO_REUSEADDR> _so_reuseaddr  { "so_reuseaddr", _sock };

		Tcpopt<GENODE_TCP_KEEPCNT>   _tcp_keepcnt   { "tcp_keepcnt"  , _sock };
		Tcpopt<GENODE_TCP_KEEPIDLE>  _tcp_keepidle  { "tcp_keepidle" , _sock };
		Tcpopt<GENODE_TCP_KEEPINTVL> _tcp_keepintvl { "tcp_keepintvl", _sock };

	public:

		Sockopt_factory(genode_socket_handle &sock) : _sock(sock) { }

		Vfs::File_system *create(Vfs::Env &, Genode::Node const &node) override
		{
			if (node.has_type(Sockopt<GENODE_SO_INVALID>::type_name())) {
				if (_so_error.matches(node))      return &_so_error;
				if (_so_keepalive.matches(node))  return &_so_keepalive;
				if (_so_reuseaddr.matches(node))  return &_so_reuseaddr;
				if (_tcp_keepcnt.matches(node))   return &_tcp_keepcnt;
				if (_tcp_keepidle.matches(node))  return &_tcp_keepidle;
				if (_tcp_keepintvl.matches(node)) return &_tcp_keepintvl;
			}

			return nullptr;
		}
};


class Vfs_ip::Sockopt_file_system : private Sockopt_factory,
                                    public Dir_file_system
{
	private:

		using Config    = Genode::String<512>;
		using Generator = Genode::Generator;

		static Config _config()
		{
			char buf[Config::capacity()] { };

			Generator::generate({ buf, sizeof(buf) }, "dir",
				[&] (Generator &g) {
					g.attribute("name", type_name());

					g.node("sockopt", [&] () { g.attribute("name", "so_error"     ); });
					g.node("sockopt", [&] () { g.attribute("name", "so_keepalive" ); });
					g.node("sockopt", [&] () { g.attribute("name", "so_reuseaddr" ); });
					g.node("sockopt", [&] () { g.attribute("name", "tcp_keepcnt"  ); });
					g.node("sockopt", [&] () { g.attribute("name", "tcp_keepidle" ); });
					g.node("sockopt", [&] () { g.attribute("name", "tcp_keepintvl"); });

			}).with_error([] (Genode::Buffer_error) {
				Genode::warning("VFS-sockopt exceeds maximum buffer size");
			});

			return Config(Genode::Cstring(buf));
		}

	public:

		Sockopt_file_system(Vfs::Env &env, genode_socket_handle &sock)
		: Sockopt_factory(sock),
		  Dir_file_system(env, Node(_config()), *this)
		{ }

		static char const *type_name() { return "sockopts"; }
};

#endif /* _SOCKOPT_H_ */

/*
 * \brief  Error-file system
 * \author Sebastian Sumpf
 * \date   2025-12-03
 *
 * This Single_file_system write a given Genode errno to a file (using
 * Genode::Node).
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#ifndef _SOCKET_ERROR_
#define _SOCKET_ERROR_

#include <genode_c_api/socket.h>
#include <vfs/single_file_system.h>


namespace Vfs_ip {
	using namespace Genode;
	using namespace Genode::Vfs;

	class Error_file_system;
}

class Vfs_ip::Error_file_system : public Single_file_system
{
	private:

		using Config = String<64>;

		static constexpr unsigned BUF_SIZE = 64;

		Errno _err             { GENODE_MAX_ERRNO };
		char  _error[BUF_SIZE] { };

		struct Vfs_handle : Single_vfs_handle
		{
			Error_file_system &fs;

			Vfs_handle(Error_file_system &fs,
			           Allocator         &alloc)
			:
				Single_vfs_handle(fs, fs, alloc, 0),
				fs(fs)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				unsigned len = min(fs.BUF_SIZE, unsigned(dst.num_bytes));
				memcpy(dst.start, &fs._error, len);
				out_count = len;

				return READ_OK;
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override {
				return Write_result::WRITE_ERR_INVALID; }

			bool read_ready()  const override { return true;  }
			bool write_ready() const override { return false; }

			private:

				Vfs_handle(Vfs_handle const &);
				Vfs_handle &operator = (Vfs_handle const &); 
		};

		Config _config() const
		{
			char buf[Config::capacity()] { };
			Generator::generate({ buf, sizeof(buf) }, type_name(),
				[] (Generator &) { }
			).with_error([&] (Buffer_error) {
				warning("VFS value fs config failed (", type_name(), ")");
			});
			return Config(Cstring(buf));
		}

	public:

		Error_file_system()
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type(),
			                   Node_rwx::rw(), Node(_config()))
		{ }

		Errno socket_error(Errno const err)
		{
			if (err == _err) return err;

			_err = err;

			Generator::generate({ _error, sizeof(_error) }, type_name(),
				[&] (Generator &g) {
					g.attribute("name", _err_string(err));
					g.attribute("value", unsigned(err));
				}
			).with_error([&] (Buffer_error) {
				warning("Error fs failed (", type_name(), ")");
			});

			return err;
		}

		static char const *type_name() { return "error"; }

		char const *type() override { return type_name(); }

		bool matches(Node const &) const
		{
			error("Error_file_system::matches");
			return false;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, Vfs::file_size size) override
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
				*out_handle = new (alloc) Vfs_handle(*this, alloc);
				return OPEN_OK;
			}
			catch (Out_of_ram)  { error("out of ram"); return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { error("out of caps");return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = BUF_SIZE;
			return result;
		}

		using Single_file_system::close;

	private:

		char const *_err_string(Errno err)
		{
			static char const *table[GENODE_MAX_ERRNO] = {
				[GENODE_ENONE]           = "ENONE",
				[GENODE_E2BIG]           = "E2BIG",
				[GENODE_EACCES]          = "EACCES",
				[GENODE_EADDRINUSE]      = "EADDRINUSE",
				[GENODE_EADDRNOTAVAIL]   = "EADDRNOTAVAIL",
				[GENODE_EAFNOSUPPORT]    = "EAFNOSUPPORT",
				[GENODE_EAGAIN]          = "EAGAIN",
				[GENODE_EALREADY]        = "EALREADY",
				[GENODE_EBADF]           = "EBADF",
				[GENODE_EBADFD]          = "EBADFD",
				[GENODE_EBADMSG]         = "EBADMSG",
				[GENODE_EBADRQC]         = "EBADRQC",
				[GENODE_EBUSY]           = "EBUSY",
				[GENODE_ECONNABORTED]    = "ECONNABORTED",
				[GENODE_ECONNREFUSED]    = "ECONNREFUSED",
				[GENODE_EDESTADDRREQ]    = "EDESTADDRREQ",
				[GENODE_EDOM]            = "EDOM",
				[GENODE_EEXIST]          = "EEXIST",
				[GENODE_EFAULT]          = "EFAULT",
				[GENODE_EFBIG]           = "EFBIG",
				[GENODE_EHOSTUNREACH]    = "EHOSTUNREACH",
				[GENODE_EINPROGRESS]     = "EINPROGRESS",
				[GENODE_EINTR]           = "EINTR",
				[GENODE_EINVAL]          = "EINVAL",
				[GENODE_EIO]             = "EIO",
				[GENODE_EISCONN]         = "EISCONN",
				[GENODE_ELOOP]           = "ELOOP",
				[GENODE_EMLINK]          = "EMLINK",
				[GENODE_EMSGSIZE]        = "EMSGSIZE",
				[GENODE_ENAMETOOLONG]    = "ENAMETOOLONG",
				[GENODE_ENETDOWN]        = "ENETDOWN",
				[GENODE_ENETUNREACH]     = "ENETUNREACH",
				[GENODE_ENFILE]          = "ENFILE",
				[GENODE_ENOBUFS]         = "ENOBUFS",
				[GENODE_ENODATA]         = "ENODATA",
				[GENODE_ENODEV]          = "ENODEV",
				[GENODE_ENOENT]          = "ENOENT",
				[GENODE_ENOIOCTLCMD]     = "ENOIOCTLCMD",
				[GENODE_ENOLINK]         = "ENOLINK",
				[GENODE_ENOMEM]          = "ENOMEM",
				[GENODE_ENOMSG]          = "ENOMSG",
				[GENODE_ENOPROTOOPT]     = "ENOPROTOOPT",
				[GENODE_ENOSPC]          = "ENOSPC",
				[GENODE_ENOSYS]          = "ENOSYS",
				[GENODE_ENOTCONN]        = "ENOTCONN",
				[GENODE_ENOTSUPP]        = "ENOTSUPP",
				[GENODE_ENOTTY]          = "ENOTTY",
				[GENODE_ENXIO]           = "ENXIO",
				[GENODE_EOPNOTSUPP]      = "EOPNOTSUPP",
				[GENODE_EOVERFLOW]       = "EOVERFLOW",
				[GENODE_EPERM]           = "EPERM",
				[GENODE_EPFNOSUPPORT]    = "EPFNOSUPPORT",
				[GENODE_EPIPE]           = "EPIPE",
				[GENODE_EPROTO]          = "EPROTO",
				[GENODE_EPROTONOSUPPORT] = "EPROTONOSUPPORT",
				[GENODE_EPROTOTYPE]      = "EPROTOTYPE",
				[GENODE_ERANGE]          = "ERANGE",
				[GENODE_EREMCHG]         = "EREMCHG",
				[GENODE_ESOCKTNOSUPPORT] = "ESOCKTNOSUPPORT",
				[GENODE_ESPIPE]          = "ESPIPE",
				[GENODE_ESRCH]           = "ESRCH",
				[GENODE_ESTALE]          = "ESTALE",
				[GENODE_ETIMEDOUT]       = "ETIMEDOUT",
				[GENODE_ETOOMANYREFS]    = "ETOOMANYREFS",
				[GENODE_EUSERS]          = "EUSERS",
				[GENODE_EXDEV]           = "EXDEV",
				[GENODE_ECONNRESET]      = "ECONNRESET",
			};

			char const *string = table[err];

			if (string == nullptr)
				warning(__func__ , ": Errno: ", unsigned(err), " is not initialized");

			return string;
		}
};

#endif /* _SOCKET_ERROR_ */

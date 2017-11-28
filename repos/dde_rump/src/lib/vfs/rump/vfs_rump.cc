/*
 * \brief  Rump VFS plugin
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Emery Hemingway
 * \date   2015-09-05
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <rump/env.h>
#include <rump_fs/fs.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>
#include <timer_session/connection.h>
#include <os/path.h>

extern "C" {
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <rump/rump.h>
#include <rump/rump_syscalls.h>
}

extern int errno;

namespace Vfs { struct Rump_file_system; };

static void _rump_sync()
{
	/* sync through front-end */
	rump_sys_sync();

	/* sync Genode back-end */
	rump_io_backend_sync();
}


static char const *fs_types[] = { RUMP_MOUNT_CD9660, RUMP_MOUNT_EXT2FS,
                                  RUMP_MOUNT_FFS, RUMP_MOUNT_MSDOS,
                                  RUMP_MOUNT_NTFS, RUMP_MOUNT_UDF, 0 };

class Vfs::Rump_file_system : public File_system
{
	private:

		enum { BUFFER_SIZE = 4096 };

		typedef Genode::Path<MAX_PATH_LEN> Path;

		Genode::Env &_env;

		struct Rump_vfs_handle : public Vfs_handle
		{
			using Vfs_handle::Vfs_handle;

			virtual Read_result read(char *buf, file_size buf_size,
			                         file_size seek_offset, file_size &out_count)
			{
				Genode::error("Rump_vfs_handle::read() called");
				return READ_ERR_INVALID;
			}

			virtual Write_result write(char const *buf, file_size buf_size,
			                           file_size seek_offset,
			                           file_size &out_count)
			{
				Genode::error("Rump_vfs_handle::write() called");
				return WRITE_ERR_INVALID;
			}
		};

		class Rump_vfs_file_handle : public Rump_vfs_handle
		{
			private:

				int _fd;

			public:

				Rump_vfs_file_handle(File_system &fs, Allocator &alloc,
				                     int status_flags, int fd)
				: Rump_vfs_handle(fs, fs, alloc, status_flags), _fd(fd) { }

				~Rump_vfs_file_handle() { rump_sys_close(_fd); }

				Ftruncate_result ftruncate(file_size len)
				{
					if (rump_sys_ftruncate(_fd, len) != 0) switch (errno) {
					case EACCES: return FTRUNCATE_ERR_NO_PERM;
					case EINTR:  return FTRUNCATE_ERR_INTERRUPT;
					case ENOSPC: return FTRUNCATE_ERR_NO_SPACE;
					default:
						return FTRUNCATE_ERR_NO_PERM;
					}
					return FTRUNCATE_OK;
				}

				Read_result read(char *buf, file_size buf_size,
				                 file_size seek_offset, file_size &out_count) override
				{
					ssize_t n = rump_sys_pread(_fd, buf, buf_size, seek_offset);
					if (n == -1) switch (errno) {
					case EWOULDBLOCK: return READ_ERR_WOULD_BLOCK;
					case EINVAL:      return READ_ERR_INVALID;
					case EIO:         return READ_ERR_IO;
					case EINTR:       return READ_ERR_INTERRUPT;
					default:
						return READ_ERR_IO;
					}
					out_count = n;
					return READ_OK;
				}

				Write_result write(char const *buf, file_size buf_size,
				                   file_size seek_offset,
				                   file_size &out_count) override
				{
					out_count = 0;

					ssize_t n = rump_sys_pwrite(_fd, buf, buf_size, seek_offset);
					if (n == -1) switch (errno) {
					case EWOULDBLOCK: return WRITE_ERR_WOULD_BLOCK;
					case EINVAL:      return WRITE_ERR_INVALID;
					case EIO:         return WRITE_ERR_IO;
					case EINTR:       return WRITE_ERR_INTERRUPT;
					default:
						return WRITE_ERR_IO;
					}
					out_count = n;
					return WRITE_OK;
				}
		};

		class Rump_vfs_dir_handle : public Rump_vfs_handle
		{
			private:

				int  _fd;
				Path _path;

				Read_result _finish_read(char const *path,
				                         struct ::dirent *dent, Dirent &vfs_dir)
				{
					/*
					 * We cannot use 'd_type' member of 'dirent' here since the EXT2
					 * implementation sets the type to unkown. Hence we use stat.
					 */
					struct stat s;
					rump_sys_lstat(path, &s);

					vfs_dir.fileno = s.st_ino;

					if (S_ISREG(s.st_mode))
						vfs_dir.type = Dirent_type::DIRENT_TYPE_FILE;
					else if (S_ISDIR(s.st_mode))
						vfs_dir.type = Dirent_type::DIRENT_TYPE_DIRECTORY;
					else if (S_ISLNK(s.st_mode))
						vfs_dir.type = Dirent_type::DIRENT_TYPE_SYMLINK;
					else if (S_ISBLK(s.st_mode))
						vfs_dir.type = Dirent_type::DIRENT_TYPE_BLOCKDEV;
					else if (S_ISCHR(s.st_mode))
						vfs_dir.type = Dirent_type::DIRENT_TYPE_CHARDEV;
					else if (S_ISFIFO(s.st_mode))
						vfs_dir.type = Dirent_type::DIRENT_TYPE_FIFO;
					else
						vfs_dir.type = Dirent_type::DIRENT_TYPE_FILE;

					strncpy(vfs_dir.name, dent->d_name, sizeof(Dirent::name));

					return READ_OK;
				}

			public:

				Rump_vfs_dir_handle(File_system &fs, Allocator &alloc,
				                    int status_flags, int fd, char const *path)
				: Rump_vfs_handle(fs, fs, alloc, status_flags),
				  _fd(fd),
				  _path(path) { }

				~Rump_vfs_dir_handle() { rump_sys_close(_fd); }

				Read_result read(char *dst, file_size count,
				                 file_size seek_offset,
				                 file_size &out_count) override
				{
					out_count = 0;

					if (count < sizeof(Dirent))
						return READ_ERR_INVALID;

					file_size index = seek_offset / sizeof(Dirent);

					Dirent *vfs_dir = (Dirent*)dst;

					out_count = sizeof(Dirent);

					rump_sys_lseek(_fd, 0, SEEK_SET);

					int bytes;
					unsigned fileno = 0;
					char *buf      = _buffer();
					struct ::dirent *dent = nullptr;
					do {
						bytes = rump_sys_getdents(_fd, buf, BUFFER_SIZE);
						void *current, *end;
						for (current = buf, end = &buf[bytes];
						     current < end;
						     current = _DIRENT_NEXT((::dirent *)current))
						{
							dent = (::dirent *)current;
							if (strcmp(".", dent->d_name) && strcmp("..", dent->d_name)) {
								if (fileno++ == index) {
									Path newpath(dent->d_name, _path.base());
									return _finish_read(newpath.base(), dent, *vfs_dir);
								}
							}
						}
					} while (bytes > 0);

					*vfs_dir = Dirent();
					return READ_OK;
				}
		};

		class Rump_vfs_symlink_handle : public Rump_vfs_handle
		{
			private:

				Path _path;

			public:

				Rump_vfs_symlink_handle(File_system &fs, Allocator &alloc,
				                        int status_flags, char const *path)
				: Rump_vfs_handle(fs, fs, alloc, status_flags), _path(path) { }

				Read_result read(char *buf, file_size buf_size,
				                 file_size seek_offset,
				                 file_size &out_count) override
				{
					out_count = 0;

					if (seek_offset != 0) {
						/* partial read is not supported */
						return READ_ERR_INVALID;
					}

					ssize_t n = rump_sys_readlink(_path.base(), buf, buf_size);
					if (n == -1)
						return READ_ERR_IO;

					out_count = n;

					return READ_OK;
				}

				Write_result write(char const *buf, file_size buf_size,
				                   file_size seek_offset,
				                   file_size &out_count) override
				{
					rump_sys_unlink(_path.base());

					if (rump_sys_symlink(buf, _path.base()) != 0) {
						out_count = 0;
						return WRITE_OK;
					}

					out_count = buf_size;
					return WRITE_OK;
				}
		};

		/**
		 * We define our own fs arg structure to fit all sizes, we assume that 'fspec'
		 * is the only valid argument and all other fields are unused.
		 */
		struct fs_args
		{
			char *fspec;
			char  pad[150];

			fs_args() { Genode::memset(pad, 0, sizeof(pad)); }
		};

		static bool _check_type(char const *type)
		{
			for (int i = 0; fs_types[i]; i++)
				if (!Genode::strcmp(type, fs_types[i]))
					return true;
			return false;
		}

		void _print_types()
		{
			Genode::error("fs types:");
			for (int i = 0; fs_types[i]; ++i)
				Genode::error("\t", fs_types[i]);
		}

		static char *_buffer()
		{
			/* buffer for directory entries */
			static char buf[BUFFER_SIZE];
			return buf;
		}

	public:

		Rump_file_system(Genode::Env &env, Xml_node const &config)
		: _env(env)
		{
			typedef Genode::String<16> Fs_type;

			Fs_type fs_type = config.attribute_value("fs", Fs_type());

			if (!_check_type(fs_type.string())) {
				Genode::error("Invalid or no file system given (use \'<rump fs=\"<fs type>\"/>)");
				_print_types();
				throw Genode::Exception();
			}

			/* mount into extra-terrestrial-file system */
			struct fs_args args;
			int opts = config.attribute_value("writeable", true) ?
				0 : RUMP_MNT_RDONLY;

			args.fspec =  (char *)GENODE_DEVICE;
			if (rump_sys_mount(fs_type.string(), "/", opts, &args, sizeof(args)) == -1) {
				Genode::error("Mounting '",fs_type,"' file system failed (",errno,")");
				throw Genode::Exception();
			}

			Genode::log(fs_type," file system mounted");
		}

		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name()   { return "rump"; }
		char const *type() override { return "rump"; }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Genode::Dataspace_capability dataspace(char const *path) override
		{
			int fd = rump_sys_open(path, O_RDONLY);
			if (fd == -1) return Genode::Dataspace_capability();

			struct stat s;
			if (rump_sys_lstat(path, &s) != 0) return Genode::Dataspace_capability();
			size_t const ds_size = s.st_size;

			char *local_addr = nullptr;
			Ram_dataspace_capability ds_cap;
			try {
				ds_cap = _env.ram().alloc(ds_size);

				local_addr = _env.rm().attach(ds_cap);

				enum { CHUNK_SIZE = 16U << 10 };

				for (size_t i = 0; i < ds_size;) {
					ssize_t n = rump_sys_read(fd, &local_addr[i], min(ds_size-i, CHUNK_SIZE));
					if (n == -1)
						throw n;
					i += n;
				}

				_env.rm().detach(local_addr);
			} catch(...) {
				if (local_addr)
					_env.rm().detach(local_addr);
				_env.ram().free(ds_cap);
			}
			rump_sys_close(fd);
			return ds_cap;
		}

		void release(char const *path,
		             Genode::Dataspace_capability ds_cap) override
		{
			if (ds_cap.valid())
				_env.ram().free(
					static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		file_size num_dirent(char const *path) override
		{
			file_size n = 0;
			int fd = rump_sys_open(*path ? path : "/", O_RDONLY | O_DIRECTORY);
			if (fd == -1)
				return 0;

			rump_sys_lseek(fd, 0, SEEK_SET);

			int   bytes = 0;
			char *buf = _buffer();
			do {
				bytes = rump_sys_getdents(fd, buf, BUFFER_SIZE);
				void *current, *end;
				for (current = buf, end = &buf[bytes];
				     current < end;
				     current = _DIRENT_NEXT((::dirent *)current))
				{
					struct ::dirent *dent = (::dirent *)current;
					if (strcmp(".", dent->d_name) && strcmp("..", dent->d_name))
						++n;
				}
			} while(bytes);

			rump_sys_close(fd);
			return n;
		}

		bool directory(char const *path) override
		{
			struct stat s;
			if (rump_sys_lstat(path, &s) != 0) return false;
			return S_ISDIR(s.st_mode);
		}

		char const *leaf_path(char const *path) override
		{
			struct stat s;
			return (rump_sys_lstat(path, &s) == 0) ? path : 0;
		}

		Open_result open(char const *path, unsigned mode,
		                 Vfs_handle **handle,
		                 Allocator  &alloc) override
		{
			/* OPEN_MODE_CREATE (or O_EXC) will not work */
			if (mode & OPEN_MODE_CREATE)
				mode |= O_CREAT;

			int fd = rump_sys_open(path, mode);
			if (fd == -1) switch (errno) {
			case ENAMETOOLONG: return OPEN_ERR_NAME_TOO_LONG;
			case EACCES:       return OPEN_ERR_NO_PERM;
			case ENOENT:       return OPEN_ERR_UNACCESSIBLE;
			case EEXIST:       return OPEN_ERR_EXISTS;
			case ENOSPC:       return OPEN_ERR_NO_SPACE;
			default:
				return OPEN_ERR_NO_PERM;
			}

			*handle = new (alloc) Rump_vfs_file_handle(*this, alloc, mode, fd);
			return OPEN_OK;
		}

		Opendir_result opendir(char const *path, bool create,
		                       Vfs_handle **handle, Allocator &alloc) override
		{
			if (strlen(path) == 0)
				path = "/";

			if (create) {
				if (rump_sys_mkdir(path, 0777) != 0) switch (::errno) {
				case ENAMETOOLONG: return OPENDIR_ERR_NAME_TOO_LONG;
				case EACCES:       return OPENDIR_ERR_PERMISSION_DENIED;
				case ENOENT:       return OPENDIR_ERR_LOOKUP_FAILED;
				case EEXIST:       return OPENDIR_ERR_NODE_ALREADY_EXISTS;
				case ENOSPC:       return OPENDIR_ERR_NO_SPACE;
				default:
					return OPENDIR_ERR_PERMISSION_DENIED;
				}
			}

			int fd = rump_sys_open(path, O_RDONLY | O_DIRECTORY);
			if (fd == -1) switch (errno) {
			case ENAMETOOLONG: return OPENDIR_ERR_NAME_TOO_LONG;
			case EACCES:       return OPENDIR_ERR_PERMISSION_DENIED;
			case ENOENT:       return OPENDIR_ERR_LOOKUP_FAILED;
			case EEXIST:       return OPENDIR_ERR_NODE_ALREADY_EXISTS;
			case ENOSPC:       return OPENDIR_ERR_NO_SPACE;
			default:
				return OPENDIR_ERR_PERMISSION_DENIED;
			}

			*handle = new (alloc) Rump_vfs_dir_handle(*this, alloc, 0777, fd, path);
			return OPENDIR_OK;

		}

		Openlink_result openlink(char const *path, bool create,
	                             Vfs_handle **handle, Allocator &alloc) override
	    {
			if (create) {
				if (rump_sys_symlink("", path) != 0) switch (errno) {
				case EEXIST:       return OPENLINK_ERR_NODE_ALREADY_EXISTS;
				case ENOENT:       return OPENLINK_ERR_LOOKUP_FAILED;
				case ENOSPC:       return OPENLINK_ERR_NO_SPACE;
				case EACCES:       return OPENLINK_ERR_PERMISSION_DENIED;
				case ENAMETOOLONG: return OPENLINK_ERR_NAME_TOO_LONG;
				default:
					return OPENLINK_ERR_PERMISSION_DENIED;
				}
			}

			char dummy;
			if (rump_sys_readlink(path, &dummy, sizeof(dummy)) == -1) switch(errno) {
				case ENOENT: return OPENLINK_ERR_LOOKUP_FAILED;
				default:
					return OPENLINK_ERR_PERMISSION_DENIED;
			}

			*handle = new (alloc) Rump_vfs_symlink_handle(*this, alloc, 0777, path);
			return OPENLINK_OK;
	    }

		void close(Vfs_handle *vfs_handle) override
		{
			if (dynamic_cast<Rump_vfs_file_handle *>(vfs_handle)
			 || dynamic_cast<Rump_vfs_dir_handle *>(vfs_handle)) {
				destroy(vfs_handle->alloc(), vfs_handle);
				return;
			}
		}

		Stat_result stat(char const *path, Stat &stat)
		{
			struct stat sb;
			if (rump_sys_lstat(path, &sb) != 0) return STAT_ERR_NO_ENTRY;

			stat.size   = sb.st_size;
			stat.mode   = sb.st_mode;
			stat.uid    = sb.st_uid;
			stat.gid    = sb.st_gid;
			stat.inode  = sb.st_ino;
			stat.device = sb.st_dev;

			return STAT_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			struct stat s;
			if (rump_sys_lstat(path, &s) == -1)
				return UNLINK_ERR_NO_ENTRY;

			if (S_ISDIR(s.st_mode)) {
				if (rump_sys_rmdir(path)  == 0) return UNLINK_OK;
			} else {
				if (rump_sys_unlink(path) == 0) return UNLINK_OK;
			}
			switch (errno) {
			case ENOENT:    return UNLINK_ERR_NO_ENTRY;
			case ENOTEMPTY: return UNLINK_ERR_NOT_EMPTY;
			}
			return UNLINK_ERR_NO_PERM;
		}

		Rename_result rename(char const *from, char const *to) override
		{
			if (rump_sys_rename(from, to) != 0) switch (errno) {
			case ENOENT: return RENAME_ERR_NO_ENTRY;
			case EXDEV:  return RENAME_ERR_CROSS_FS;
			case EACCES: return RENAME_ERR_NO_PERM;
			}
			return RENAME_OK;
		}


		/*******************************
		 ** File io service interface **
		 *******************************/

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *buf, file_size buf_size,
		                   file_size &out_count) override
		{
			Rump_vfs_handle *handle =
				static_cast<Rump_vfs_handle *>(vfs_handle);

			if (handle)
				return handle->write(buf, buf_size, handle->seek(), out_count);

			return WRITE_ERR_INVALID;
		}

		Read_result complete_read(Vfs_handle *vfs_handle, char *buf,
		                          file_size buf_size,
		                          file_size &out_count) override
		{
			Rump_vfs_handle *handle =
				static_cast<Rump_vfs_handle *>(vfs_handle);

			if (handle)
				return handle->read(buf, buf_size, handle->seek(), out_count);

			return READ_ERR_INVALID;
		}

		bool read_ready(Vfs_handle *) override { return true; }

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len) override
		{
			Rump_vfs_file_handle *handle =
				dynamic_cast<Rump_vfs_file_handle *>(vfs_handle);

			if (handle)
				return handle->ftruncate(len);

			return FTRUNCATE_ERR_NO_PERM;
		}

		Sync_result complete_sync(Vfs_handle *) override
		{
			_rump_sync();
			return SYNC_OK;
		}
};


class Rump_factory : public Vfs::File_system_factory
{
	private:

		Timer::Connection                       _timer;
		Genode::Io_signal_handler<Rump_factory> _sync_handler;

		void _sync() { _rump_sync(); }

	public:

		Rump_factory(Genode::Env &env, Genode::Allocator &alloc)
		: _timer(env, "rump-sync"),
		  _sync_handler(env.ep(), *this, &Rump_factory::_sync)
		{
			Rump::construct_env(env);

			rump_io_backend_init();

			/* start rump kernel */
			rump_init();

			/* register block device */
			rump_pub_etfs_register(
				GENODE_DEVICE, GENODE_BLOCK_SESSION, RUMP_ETFS_BLK);

			/* set all bits but the stickies */
			rump_sys_umask(S_ISUID|S_ISGID|S_ISVTX);

			/* start syncing */
			enum { TEN_SEC = 10*1000*1000 };
			_timer.sigh(_sync_handler);
			_timer.trigger_periodic(TEN_SEC);

		}

		Vfs::File_system *create(Genode::Env       &env,
		                         Genode::Allocator &alloc,
		                         Genode::Xml_node   config,
		                         Vfs::Io_response_handler &) override
		{
			return new (alloc) Vfs::Rump_file_system(env, config);
		}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Extern_factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Genode::Env &env,
		                         Genode::Allocator &alloc,
		                         Genode::Xml_node node,
		                         Vfs::Io_response_handler &io_handler) override
		{
			static Rump_factory factory(env, alloc);
			return factory.create(env, alloc, node, io_handler);
		}
	};

	static Extern_factory factory;
	return &factory;
}

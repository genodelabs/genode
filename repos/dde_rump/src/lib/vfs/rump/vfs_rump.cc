/*
 * \brief  Rump VFS plugin
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Emery Hemingway
 * \date   2015-09-05
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <rump/env.h>
#include <rump_fs/fs.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>
#include <os/path.h>

extern "C" {
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/event.h>
#include <sys/time.h>
#include <rump/rump.h>
#include <rump/rump_syscalls.h>
}

extern int errno;

namespace Vfs { struct Rump_file_system; };

static void _rump_sync()
{
	/* prevent nested calls into rump */
	if (rump_io_backend_blocked_for_io())
		return;

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

		Vfs::Env &_env;

		struct Rump_vfs_dir_handle;
		struct Rump_watch_handle;
		typedef Genode::List<Rump_watch_handle> Rump_watch_handles;
		Rump_watch_handles _watchers { };

		struct Rump_vfs_file_handle;
		typedef Genode::List<Rump_vfs_file_handle> Rump_vfs_file_handles;
		Rump_vfs_file_handles _file_handles;

		struct Rump_vfs_handle : public Vfs_handle
		{
			using Vfs_handle::Vfs_handle;

			virtual Read_result read(Byte_range_ptr const &dst,
			                         file_size seek_offset, size_t &out_count)
			{
				Genode::error("Rump_vfs_handle::read() called");
				return READ_ERR_INVALID;
			}

			virtual Write_result write(Const_byte_range_ptr const &src,
			                           file_size seek_offset,
			                           size_t &out_count)
			{
				Genode::error("Rump_vfs_handle::write() called");
				return WRITE_ERR_INVALID;
			}

			virtual void update_modification_timestamp(Vfs::Timestamp) { }
		};

		class Rump_vfs_file_handle :
			public Rump_vfs_handle, public Rump_vfs_file_handles::Element
		{
			private:

				int _fd;
				bool _modifying = false;

			public:

				Rump_vfs_file_handle(File_system &fs, Allocator &alloc,
				                     int status_flags, int fd)
				: Rump_vfs_handle(fs, fs, alloc, status_flags), _fd(fd)
				{ }

				~Rump_vfs_file_handle() { rump_sys_close(_fd); }

				bool modifying() const { return _modifying; }

				Ftruncate_result ftruncate(file_size len)
				{
					if (rump_sys_ftruncate(_fd, len) != 0) switch (errno) {
					case EACCES: return FTRUNCATE_ERR_NO_PERM;
					case EINTR:  return FTRUNCATE_ERR_INTERRUPT;
					case ENOSPC: return FTRUNCATE_ERR_NO_SPACE;
					default:
						Genode::error(__func__, ": unhandled rump error ", errno);
						return FTRUNCATE_ERR_NO_PERM;
					}
					_modifying = true;
					return FTRUNCATE_OK;
				}

				Read_result read(Byte_range_ptr const &dst,
				                 file_size seek_offset, size_t &out_count) override
				{
					ssize_t n = rump_sys_pread(_fd, dst.start, dst.num_bytes, seek_offset);
					if (n == -1) switch (errno) {
					case EWOULDBLOCK: return READ_ERR_WOULD_BLOCK;
					case EINVAL:      return READ_ERR_INVALID;
					case EIO:         return READ_ERR_IO;
					case EINTR:       return READ_ERR_IO;
					default:
						Genode::error(__func__, ": unhandled rump error ", errno);
						return READ_ERR_IO;
					}
					out_count = n;
					return READ_OK;
				}

				Write_result write(Const_byte_range_ptr const &src,
				                   file_size seek_offset, size_t &out_count) override
				{
					out_count = 0;

					ssize_t n = rump_sys_pwrite(_fd, src.start, src.num_bytes, seek_offset);
					if (n == -1) switch (errno) {
					case EWOULDBLOCK: return WRITE_ERR_WOULD_BLOCK;
					case EINVAL:      return WRITE_ERR_INVALID;
					case EIO:         return WRITE_ERR_IO;
					case EINTR:       return WRITE_ERR_IO;
					default:
						Genode::error(__func__, ": unhandled rump error ", errno);
						return WRITE_ERR_IO;
					}
					_modifying = true;
					out_count = n;
					return WRITE_OK;
				}

				void update_modification_timestamp(Vfs::Timestamp time) override
				{
					struct timespec ts[2] = {
						{ .tv_sec = 0,          .tv_nsec = 0 },
						{ .tv_sec = time.value, .tv_nsec = 0 }
					};

					/* silently igore error */
					rump_sys_futimens(_fd, (const timespec*)&ts);
				}
		};

		class Rump_vfs_dir_handle : public Rump_vfs_handle
		{
			private:
				int _fd;
			public:
				Path const path;

			private:

				Read_result _finish_read(char const *path,
				                         struct ::dirent *dent, Dirent &vfs_dir)
				{
					/*
					 * We cannot use 'd_type' member of 'dirent' here since the EXT2
					 * implementation sets the type to unkown. Hence we use stat.
					 */
					struct stat s { };
					rump_sys_lstat(path, &s);

					auto dirent_type = [] (unsigned mode)
					{
						if (S_ISREG (mode)) return Dirent_type::CONTINUOUS_FILE;
						if (S_ISDIR (mode)) return Dirent_type::DIRECTORY;
						if (S_ISLNK (mode)) return Dirent_type::SYMLINK;
						if (S_ISBLK (mode)) return Dirent_type::CONTINUOUS_FILE;
						if (S_ISCHR (mode)) return Dirent_type::CONTINUOUS_FILE;
						if (S_ISFIFO(mode)) return Dirent_type::CONTINUOUS_FILE;

						return Dirent_type::END;
					};

					Node_rwx const rwx { .readable   = true,
					                     .writeable  = true,
					                     .executable = (s.st_mode & S_IXUSR) != 0 };

					vfs_dir = {
						.fileno = s.st_ino,
						.type   = dirent_type(s.st_mode),
						.rwx    = rwx,
						.name   = { dent->d_name }
					};
					return READ_OK;
				}

			public:

				Rump_vfs_dir_handle(File_system &fs, Allocator &alloc,
				                    int status_flags, int fd, char const *path)
				: Rump_vfs_handle(fs, fs, alloc, status_flags),
				  _fd(fd), path(path) { }

				~Rump_vfs_dir_handle() { rump_sys_close(_fd); }

				Read_result read(Byte_range_ptr const &dst,
				                 file_size seek_offset, size_t &out_count) override
				{
					out_count = 0;

					if (dst.num_bytes < sizeof(Dirent))
						return READ_ERR_INVALID;

					size_t const index = size_t(seek_offset / sizeof(Dirent));

					Dirent *vfs_dir = (Dirent*)dst.start;

					out_count = sizeof(Dirent);

					rump_sys_lseek(_fd, 0, SEEK_SET);

					int bytes;
					unsigned fileno = 0;
					char *buf  = _buffer();
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
									Path newpath(dent->d_name, path.base());
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

				Path const _path;

			public:

				Rump_vfs_symlink_handle(File_system &fs, Allocator &alloc,
				                        int status_flags, char const *path)
				: Rump_vfs_handle(fs, fs, alloc, status_flags), _path(path) { }

				Read_result read(Byte_range_ptr const &dst,
				                 file_size seek_offset, size_t &out_count) override
				{
					out_count = 0;

					if (seek_offset != 0) {
						/* partial read is not supported */
						return READ_ERR_INVALID;
					}

					ssize_t n = rump_sys_readlink(_path.base(), dst.start, dst.num_bytes);
					if (n == -1)
						return READ_ERR_IO;

					out_count = n;

					return READ_OK;
				}

				Write_result write(Const_byte_range_ptr const &src,
				                   file_size seek_offset, size_t &out_count) override
				{
					rump_sys_unlink(_path.base());

					if (rump_sys_symlink(src.start, _path.base()) != 0) {
						out_count = 0;
						return WRITE_OK;
					}

					out_count = src.num_bytes;
					return WRITE_OK;
				}
		};

		struct Rump_watch_handle : Vfs_watch_handle, Rump_watch_handles::Element
		{
			int fd, kq;

			Rump_watch_handle(Vfs::File_system &fs,
			                  Allocator        &alloc,
			                  int              &fd)
			: Vfs_watch_handle(fs, alloc), fd(fd)
			{
				struct kevent ev;
				struct timespec nullts = { 0, 0 };
				EV_SET(&ev, fd, EVFILT_VNODE,
				       EV_ADD|EV_ENABLE|EV_CLEAR,
				       NOTE_DELETE|NOTE_WRITE|NOTE_RENAME,
				       0, 0);
				kq = rump_sys_kqueue();
				rump_sys_kevent(kq, &ev, 1, NULL, 0, &nullts);
			}

			~Rump_watch_handle()
			{
				rump_sys_close(fd);
				rump_sys_close(kq);
			}

			bool kqueue_check() const
			{
				struct kevent ev;
				struct timespec nullts = { 0, 0 };

				int n = rump_sys_kevent(
					kq, NULL, 0, &ev, 1, &nullts);
				return (n > 0);
			}
		};

		/*
		 * Must fit 'struct msdosfs_args' and 'struct ufs_args'. Needed to
		 * pass mount flags the file-system drivers.
		 */
		struct fs_args
		{
			char *fspec;

			/* unused */
			struct export_args30 _pad1;
			uid_t                _uid;
			gid_t                _gid;
			mode_t               _mask;

			int flags;

			char _pad[164];
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

		/**
		 * Notify the application for each handle on a modified file.
		 */
		void _notify_files()
		{
			for (Rump_watch_handle *h = _watchers.first(); h; h = h->next()) {
				if (h->kqueue_check())
					h->watch_response();
			}
		}

	public:

		Rump_file_system(Vfs::Env &env, Xml_node const &config)
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
			struct fs_args args { };

			if (fs_type == "msdos" && config.attribute_value("gemdos", false)) {
				enum { MSDOSFSMNT_GEMDOSFS = 8 };
				args.flags |= MSDOSFSMNT_GEMDOSFS;
			}

			int opts = config.attribute_value("writeable", true) ?
				0 : RUMP_MNT_RDONLY;

			args.fspec =  (char *)GENODE_DEVICE;
			if (rump_sys_mount(fs_type.string(), "/", opts, &args, sizeof(args)) == -1) {
				Genode::error("Mounting '",fs_type,"' file system failed (",errno,")");
				throw Genode::Exception();
			}
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
			Genode::Env &env = _env.env();

			int fd = rump_sys_open(path, O_RDONLY);
			if (fd == -1) return Genode::Dataspace_capability();

			struct stat s;
			if (rump_sys_lstat(path, &s) != 0) return Genode::Dataspace_capability();
			size_t const ds_size = s.st_size;

			char *local_addr = nullptr;
			Ram_dataspace_capability ds_cap;
			try {
				ds_cap = env.ram().alloc(ds_size);

				local_addr = env.rm().attach(ds_cap);

				enum { CHUNK_SIZE = 16U << 10 };

				for (size_t i = 0; i < ds_size;) {
					ssize_t n = rump_sys_read(fd, &local_addr[i], min(ds_size-i, CHUNK_SIZE));
					if (n == -1)
						throw n;
					i += n;
				}

				env.rm().detach(local_addr);
			} catch(...) {
				if (local_addr)
					env.rm().detach(local_addr);
				env.ram().free(ds_cap);
			}
			rump_sys_close(fd);
			return ds_cap;
		}

		void release(char const *path,
		             Genode::Dataspace_capability ds_cap) override
		{
			if (ds_cap.valid())
				_env.env().ram().free(
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
			bool create = mode & OPEN_MODE_CREATE;
			if (create)
				mode |= O_CREAT;

			enum { DEFAULT_PERMISSIONS = 0777 };
			int fd = create ? rump_sys_open(path, mode, DEFAULT_PERMISSIONS) : rump_sys_open(path, mode);
			if (fd == -1) switch (errno) {
			case ENAMETOOLONG: return OPEN_ERR_NAME_TOO_LONG;
			case EACCES:       return OPEN_ERR_NO_PERM;
			case ENOENT:       return OPEN_ERR_UNACCESSIBLE;
			case EEXIST:       return OPEN_ERR_EXISTS;
			case ENOSPC:       return OPEN_ERR_NO_SPACE;
			default:
				Genode::error(__func__, ": unhandled rump error ", errno);
				return OPEN_ERR_NO_PERM;
			}

			if (create)
				_notify_files();

			try {
				Rump_vfs_file_handle *h = new (alloc)
					Rump_vfs_file_handle(*this, alloc, mode, fd);
				*handle = h;
				return OPEN_OK;
			} catch (Genode::Out_of_ram) {
				rump_sys_close(fd);
				return OPEN_ERR_OUT_OF_RAM;
			} catch (Genode::Out_of_caps) {
				rump_sys_close(fd);
				return OPEN_ERR_OUT_OF_CAPS;
			}
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
					Genode::error(__func__, ": unhandled rump error ", errno);
					return OPENDIR_ERR_PERMISSION_DENIED;
				}

				_notify_files();
			}

			int fd = rump_sys_open(path, O_RDONLY | O_DIRECTORY);
			if (fd == -1) switch (errno) {
			case ENAMETOOLONG: return OPENDIR_ERR_NAME_TOO_LONG;
			case EACCES:       return OPENDIR_ERR_PERMISSION_DENIED;
			case ENOENT:       return OPENDIR_ERR_LOOKUP_FAILED;
			case EEXIST:       return OPENDIR_ERR_NODE_ALREADY_EXISTS;
			case ENOSPC:       return OPENDIR_ERR_NO_SPACE;
			default:
				Genode::error(__func__, ": unhandled rump error ", errno);
				return OPENDIR_ERR_PERMISSION_DENIED;
			}

			try {
				Rump_vfs_dir_handle *h = new (alloc)
					Rump_vfs_dir_handle(*this, alloc, 0777, fd, path);
				*handle = h;
				return OPENDIR_OK;
			} catch (Genode::Out_of_ram) {
				rump_sys_close(fd);
				return OPENDIR_ERR_OUT_OF_RAM;
			} catch (Genode::Out_of_caps) {
				rump_sys_close(fd);
				return OPENDIR_ERR_OUT_OF_CAPS;
			}
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
					Genode::error(__func__, ": unhandled rump error ", errno);
					return OPENLINK_ERR_PERMISSION_DENIED;
				}

				_notify_files();
			}

			char dummy;
			if (rump_sys_readlink(path, &dummy, sizeof(dummy)) == -1) switch(errno) {
				case ENOENT: return OPENLINK_ERR_LOOKUP_FAILED;
				default:
					Genode::error(__func__, ": unhandled rump error ", errno);
					return OPENLINK_ERR_PERMISSION_DENIED;
			}

			try {
				*handle = new (alloc) Rump_vfs_symlink_handle(*this, alloc, 0777, path);
				return OPENLINK_OK;
			}
			catch (Genode::Out_of_ram) { return OPENLINK_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPENLINK_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_handle *vfs_handle) override
		{
			if (Rump_vfs_file_handle *handle =
				dynamic_cast<Rump_vfs_file_handle *>(vfs_handle))
			{
				_file_handles.remove(handle);
				if (handle->modifying())
					_notify_files();
				destroy(vfs_handle->alloc(), handle);
			}
			else
			if (Rump_vfs_dir_handle *handle =
				dynamic_cast<Rump_vfs_dir_handle *>(vfs_handle))
			{
				destroy(vfs_handle->alloc(), handle);
			}
			else
			if (Rump_vfs_symlink_handle *handle =
				dynamic_cast<Rump_vfs_symlink_handle *>(vfs_handle))
			{
				destroy(vfs_handle->alloc(), handle);
			}
		}

		Stat_result stat(char const *path, Stat &stat)
		{
			struct stat sb { };
			if (rump_sys_lstat(path, &sb) != 0) return STAT_ERR_NO_ENTRY;

			auto type = [] (unsigned mode)
			{
				if (S_ISDIR(mode)) return Node_type::DIRECTORY;
				if (S_ISLNK(mode)) return Node_type::SYMLINK;

				return Node_type::CONTINUOUS_FILE;
			};

			stat = {
				.size   = (file_size)sb.st_size,
				.type   = type(sb.st_mode),
				.rwx    = { .readable   = true,
				            .writeable  = true,
				            .executable = (sb.st_mode & S_IXUSR) != 0 },
				.inode  = sb.st_ino,
				.device = sb.st_dev,

				.modification_time = { 0 }
			};

			return STAT_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			struct stat s;
			if (rump_sys_lstat(path, &s) == -1)
				return UNLINK_ERR_NO_ENTRY;

			int const r = S_ISDIR(s.st_mode)
				? rump_sys_rmdir(path)
				: rump_sys_unlink(path);

			if (r != 0) switch (errno) {
			case ENOENT:    return UNLINK_ERR_NO_ENTRY;
			case ENOTEMPTY: return UNLINK_ERR_NOT_EMPTY;
			default:
				Genode::error(__func__, ": unhandled rump error ", errno);
				return UNLINK_ERR_NO_PERM;
			}

			_notify_files();
			return UNLINK_OK;
		}

		Rename_result rename(char const *from, char const *to) override
		{
			if (rump_sys_rename(from, to) != 0) switch (errno) {
			case ENOENT: return RENAME_ERR_NO_ENTRY;
			case EXDEV:  return RENAME_ERR_CROSS_FS;
			case EACCES: return RENAME_ERR_NO_PERM;
			}

			_notify_files();
			return RENAME_OK;
		}

		Watch_result watch(char const      *path,
		                   Vfs_watch_handle **handle,
		                   Allocator        &alloc) override
		{
			int fd = rump_sys_open(path, O_RDONLY);
			if (fd < 0)
				return WATCH_ERR_UNACCESSIBLE;

			try {
				auto *watch_handle = new (alloc)
					Rump_watch_handle(*this, alloc, fd);
				_watchers.insert(watch_handle);
				*handle = watch_handle;
				return WATCH_OK;
			}
			catch (Genode::Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *vfs_handle) override
		{
			auto *watch_handle =
				static_cast<Rump_watch_handle *>(vfs_handle);
			_watchers.remove(watch_handle);
			destroy(watch_handle->alloc(), watch_handle);
		};


		/*******************************
		 ** File io service interface **
		 *******************************/

		Write_result write(Vfs_handle *vfs_handle, Const_byte_range_ptr const &src,
		                   size_t &out_count) override
		{
			Rump_vfs_handle *handle =
				static_cast<Rump_vfs_handle *>(vfs_handle);

			if (handle)
				return handle->write(src, handle->seek(), out_count);

			return WRITE_ERR_INVALID;
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          Byte_range_ptr const &dst,
		                          size_t &out_count) override
		{
			Rump_vfs_handle *handle =
				static_cast<Rump_vfs_handle *>(vfs_handle);

			if (handle)
				return handle->read(dst, handle->seek(), out_count);

			return READ_ERR_INVALID;
		}

		bool read_ready (Vfs_handle const &) const override { return true; }
		bool write_ready(Vfs_handle const &) const override { return true; }

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len) override
		{
			Rump_vfs_file_handle *handle =
				dynamic_cast<Rump_vfs_file_handle *>(vfs_handle);

			if (handle)
				return handle->ftruncate(len);

			return FTRUNCATE_ERR_NO_PERM;
		}

		Sync_result complete_sync(Vfs_handle *vfs_handle) override
		{
			_rump_sync();
			Rump_vfs_file_handle *handle =
				static_cast<Rump_vfs_file_handle *>(vfs_handle);
			if (handle && handle->modifying())
				_notify_files();
			return SYNC_OK;
		}

		bool update_modification_timestamp(Vfs_handle *vfs_handle,
		                                   Vfs::Timestamp ts) override
		{
			Rump_vfs_file_handle *handle =
				dynamic_cast<Rump_vfs_file_handle *>(vfs_handle);
			if (handle)
				handle->update_modification_timestamp(ts);

			return true;
		}
};


class Rump_factory : public Vfs::File_system_factory
{
	private:

		struct Rump_fs_user : Rump_fs_user_wakeup
		{
			Vfs::Env::User &_vfs_user;

			void wakeup_rump_fs_user() override { _vfs_user.wakeup_vfs_user(); }

			Rump_fs_user(Vfs::Env::User &vfs_user) : _vfs_user(vfs_user) { }

		} _rump_fs_user;

	public:

		Rump_factory(Genode::Env &env, Genode::Allocator &alloc,
		             Vfs::Env::User &vfs_user, Genode::Xml_node config)
		:
			_rump_fs_user(vfs_user)
		{
			Rump::construct_env(env);

			rump_io_backend_init(_rump_fs_user);

			/* limit RAM consumption */
			if (!config.has_attribute("ram")) {
				Genode::error("mandatory 'ram' attribute missing");
				throw Genode::Exception();
			}

			Genode::Number_of_bytes const memlimit =
				config.attribute_value("ram", Genode::Number_of_bytes(0));

			rump_set_memlimit(memlimit);

			/* start rump kernel */
			try         { rump_init(); }
			catch (...) { throw Genode::Exception(); }

			/* register block device */
			rump_pub_etfs_register(
				GENODE_DEVICE, GENODE_BLOCK_SESSION, RUMP_ETFS_BLK);

			/* set all bits but the stickies */
			rump_sys_umask(S_ISUID|S_ISGID|S_ISVTX);

			/* increase open file limit */
			struct rlimit rlim { ~0U, ~0U };
			if (rump_sys_getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
				rlim.rlim_cur = rlim.rlim_max;
				rump_sys_setrlimit(RLIMIT_NOFILE, &rlim);
			}
		}

		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
		{
			return new (env.alloc()) Vfs::Rump_file_system(env, config);
		}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Extern_factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node node) override
		{
			static Rump_factory factory(env.env(), env.alloc(), env.user(), node);
			return factory.create(env, node);
		}
	};

	static Extern_factory factory;
	return &factory;
}

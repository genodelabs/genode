/*
 * \brief  TAR file system
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__TAR_FILE_SYSTEM_H_
#define _NOUX__TAR_FILE_SYSTEM_H_

/* Genode includes */
#include <base/printf.h>
#include <base/lock.h>
#include <rom_session/connection.h>
#include <dataspace/client.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include <file_system.h>
#include <vfs_handle.h>

namespace Noux {

	class Tar_file_system : public File_system
	{
		enum { verbose = false };

		Lock _lock;

		struct Rom_name
		{
			enum { ROM_NAME_MAX_LEN = 64 };
			char name[ROM_NAME_MAX_LEN];

			Rom_name(Xml_node config) {
				config.attribute("name").value(name, sizeof(name));
			}
		} _rom_name;

		Rom_connection _rom;

		char  *_tar_base;
		size_t _tar_size;

		class Record
		{
			private:

				char _name[100];
				char _mode[8];
				char _uid[8];
				char _gid[8];
				char _size[12];
				char _mtime[12];
				char _checksum[8];
				char _type[1];
				char _linked_name[100];

				/**
				 * Convert ASCII-encoded octal number to unsigned value
				 */
				template <typename T>
				unsigned long _read(T const &field) const
				{
					/*
					 * Copy-out ASCII string to temporary buffer that is
					 * large enough to host an additional zero.
					 */
					char buf[sizeof(field) + 1];
					strncpy(buf, field, sizeof(buf));

					unsigned long value = 0;
					ascii_to(buf, &value, 8);
					return value;
				}

			public:

				/* length of on data block in tar */
				enum { BLOCK_LEN = 512 };

				/* record type values */
				enum { TYPE_FILE    = 0, TYPE_HARDLINK = 1,
				       TYPE_SYMLINK = 2, TYPE_DIR      = 5 };

				size_t             size() const { return _read(_size); }
				unsigned            uid() const { return _read(_uid);  }
				unsigned            gid() const { return _read(_gid);  }
				unsigned           mode() const { return _read(_mode); }
				unsigned           type() const { return _read(_type); }
				char const        *name() const { return _name;        }
				char const *linked_name() const { return _linked_name; }

				void *data() const { return (char *)this + BLOCK_LEN; }
		};


		class Tar_vfs_handle : public Vfs_handle
		{
			private:

				Record const *_record;

			public:

				Tar_vfs_handle(File_system *fs, int status_flags, Record *record)
				: Vfs_handle(fs, fs, status_flags), _record(record)
				{ }

				Record const *record() const { return _record; }
		};


		struct Lookup_criterion { virtual bool match(char const *path) = 0; };


		/**
		 * Return portion of 'path' without leading slashes and dots
		 */
		static char const *_skip_leading_slashes_dots(char const *path)
		{
			while (*path == '.' || *path == '/')
				path++;

			return path;
		}


		static void _remove_trailing_slashes(char *str)
		{
			size_t len = 0;
			while ((len = strlen(str)) && (str[len - 1] == '/'))
				str[len - 1] = 0;
		}


		struct Lookup_exact : public Lookup_criterion
		{
			Absolute_path _match_path;

			Lookup_exact(char const *match_path)
			: _match_path(match_path)
			{
				_match_path.remove_trailing('/');
			}

			bool match(char const *path)
			{
				Absolute_path test_path(path);
				test_path.remove_trailing('/');
				return _match_path.equals(test_path);
			}
		};


		/**
		 * Lookup the Nth record in the specified path
		 */
		struct Lookup_member_of_path : public Lookup_criterion
		{
			Absolute_path _dir_path;
			int const index;
			int cnt;

			Lookup_member_of_path(char const *dir_path, int index)
			: _dir_path(dir_path), index(index), cnt(0)
			{
				_dir_path.remove_trailing('/');
			}

			bool match(char const *path)
			{
				Absolute_path test_path(path);

				if (!test_path.strip_prefix(_dir_path.base()))
					return false;

				if (!test_path.has_single_element())
					return false;

				cnt++;

				/* match only if index is reached */
				if (cnt - 1 != index)
					return false;

				return true;
			}
		};


		Record *_lookup(Lookup_criterion *criterion)
		{
			/* measure size of archive in blocks */
			unsigned block_id = 0, block_cnt = _tar_size/Record::BLOCK_LEN;

			/* scan metablocks of archive */
			while (block_id < block_cnt) {

				Record *record = (Record *)(_tar_base + block_id*Record::BLOCK_LEN);

				/* get infos about current file */
				if (criterion->match(record->name()))
					return record;

				size_t file_size = record->size();

				/* some datablocks */       /* one metablock */
				block_id = block_id + (file_size / Record::BLOCK_LEN) + 1;

				/* round up */
				if (file_size % Record::BLOCK_LEN != 0) block_id++;

				/* check for end of tar archive */
				if (block_id*Record::BLOCK_LEN >= _tar_size)
					break;

				/* lookout for empty eof-blocks */
				if (*(_tar_base + (block_id*Record::BLOCK_LEN)) == 0x00)
					if (*(_tar_base + (block_id*Record::BLOCK_LEN + 1)) == 0x00)
						break;
			}

			return 0;
		}


		struct Num_dirent_cache
		{
			Lock             lock;
			Tar_file_system &tar_fs;
			bool             valid;              /* true after first lookup */
			char             key[256];           /* key used for lookup */
			size_t           cached_num_dirent;  /* cached value */

			Num_dirent_cache(Tar_file_system &tar_fs)
			: tar_fs(tar_fs), valid(false), cached_num_dirent(0) { }

			size_t num_dirent(char const *path)
			{
				Lock::Guard guard(lock);

				/* check for cache miss */
				if (!valid || strcmp(path, key) != 0) {

					Lookup_member_of_path lookup_criterion(path, ~0);
					tar_fs._lookup(&lookup_criterion);
					strncpy(key, path, sizeof(key));
					cached_num_dirent = lookup_criterion.cnt;
					valid = true;
				}
				return cached_num_dirent;
			}
		} _cached_num_dirent;


		public:

			Tar_file_system(Xml_node config)
			:
				_rom_name(config), _rom(_rom_name.name),
				_tar_base(env()->rm_session()->attach(_rom.dataspace())),
				_tar_size(Dataspace_client(_rom.dataspace()).size()),
				_cached_num_dirent(*this)
			{
				PINF("tar archive '%s' local at %p, size is %zd",
				     _rom_name.name, _tar_base, _tar_size);
			}


			/*********************************
			 ** Directory-service interface **
			 *********************************/

			Dataspace_capability dataspace(char const *path)
			{
				/*
				 * Walk hardlinks until we reach a file
				 */
				Record *record = 0;
				for (;;) {
					Lookup_exact lookup_criterion(path);
					record = _lookup(&lookup_criterion);
					if (!record)
						return Dataspace_capability();

					if (record->type() == Record::TYPE_HARDLINK) {
						path = record->linked_name();
						continue;
					}

					if (record->type() == Record::TYPE_FILE)
						break;

					PERR("TAR record \"%s\" has unsupported type %d",
					     record->name(), record->type());
					return Dataspace_capability();
				}

				try {
					Ram_dataspace_capability ds_cap =
						env()->ram_session()->alloc(record->size());

					void *local_addr = env()->rm_session()->attach(ds_cap);
					memcpy(local_addr, record->data(), record->size());
					env()->rm_session()->detach(local_addr);

					return ds_cap;
				}
				catch (...) { PDBG("Could not create new dataspace"); }

				return Dataspace_capability();
			}

			void release(char const *, Dataspace_capability ds_cap)
			{
				env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds_cap));
			}

			bool stat(Sysio *sysio, char const *path)
			{
				Lookup_exact lookup_criterion(path);
				Record *record = _lookup(&lookup_criterion);
				if (!record) {
					sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
					return false;
				}

				/* convert TAR record modes to stat modes */
				unsigned mode = record->mode();
				switch (record->type()) {
				case Record::TYPE_FILE:    mode |= Sysio::STAT_MODE_FILE; break;
				case Record::TYPE_SYMLINK: mode |= Sysio::STAT_MODE_SYMLINK; break;
				case Record::TYPE_DIR:     mode |= Sysio::STAT_MODE_DIRECTORY; break;

				default:
					if (verbose)
						PDBG("unhandled record type %d", record->type());
				}

				memset(&sysio->stat_out.st, 0, sizeof(sysio->stat_out.st));
				sysio->stat_out.st.mode   = mode;
				sysio->stat_out.st.size   = record->size();
				sysio->stat_out.st.uid    = record->uid();
				sysio->stat_out.st.gid    = record->gid();
				sysio->stat_out.st.inode  = (unsigned long)record;
				return true;
			}

			bool dirent(Sysio *sysio, char const *path, off_t index)
			{
				Lock::Guard guard(_lock);

				Lookup_member_of_path lookup_criterion(path, index);
				Record *record = _lookup(&lookup_criterion);
				if (!record) {
					sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
					return true;
				}

				sysio->dirent_out.entry.fileno = (unsigned long)record;

				switch (record->type()) {
				case 0: sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_FILE;      break;
				case 2: sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_SYMLINK;   break;
				case 5: sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_DIRECTORY; break;

				default:
					if (verbose)
						PDBG("unhandled record type %d", record->type());
				}

				Absolute_path absolute_path(record->name());
				absolute_path.keep_only_last_element();
				absolute_path.remove_trailing('/');

				strncpy(sysio->dirent_out.entry.name,
				        absolute_path.base() + 1,
				        sizeof(sysio->dirent_out.entry.name));

				return true;
			}

			bool unlink(Sysio *, char const *) { return false; }

			bool rename(Sysio *, char const *, char const *) { return false; }

			bool mkdir(Sysio *, char const *) { return false; }

			size_t num_dirent(char const *path)
			{
				return _cached_num_dirent.num_dirent(path);
			}

			bool is_directory(char const *path)
			{
				Lookup_exact lookup_criterion(path);
				Record const * record = _lookup(&lookup_criterion);
				return record && (record->type() == Record::TYPE_DIR);
			}

			char const *leaf_path(char const *path)
			{
				/*
				 * Check if path exists within the file system. If this is the
				 * case, return the whole path, which is relative to the root
				 * of this file system.
				 */
				Lookup_exact lookup_criterion(path);
				Record const * record = _lookup(&lookup_criterion);
				return record ? path : 0;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				Lock::Guard guard(_lock);

				Lookup_exact lookup_criterion(path);
				Record *record = 0;
				if ((record = _lookup(&lookup_criterion)))
					return new (env()->heap())
						Tar_vfs_handle(this, 0, record);

				sysio->error.open = Sysio::OPEN_ERR_UNACCESSIBLE;
				return 0;
			}


			/***************************
			 ** File_system interface **
			 ***************************/

			char const *name() const { return "tar"; }


			/********************************
			 ** File I/O service interface **
			 ********************************/

			bool write(Sysio *sysio, Vfs_handle *handle)
			{
				PDBG("called\n");
				return false;
			}

			bool read(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				Tar_vfs_handle const *handle = static_cast<Tar_vfs_handle *>(vfs_handle);

				size_t const record_size = handle->record()->size();

				size_t const record_bytes_left = record_size >= handle->seek()
				                               ? record_size  - handle->seek() : 0;

				size_t const count = min(record_bytes_left,
				                         min(sizeof(sysio->read_out.chunk),
				                             sysio->read_in.count));

				char const *data = (char *)handle->record()->data() + handle->seek();

				memcpy(sysio->read_out.chunk, data, count);

				sysio->read_out.count = count;
				return true;
			}

			bool ftruncate(Sysio *sysio, Vfs_handle *handle)
			{
				PDBG("called\n");
				return false;
			}
	};
}

#endif /* _NOUX__TAR_FILE_SYSTEM_H_ */

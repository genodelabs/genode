/*
 * \brief  TAR file system
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
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

				Tar_vfs_handle(File_system *fs, int status_flags, Record const *record)
				: Vfs_handle(fs, fs, status_flags), _record(record)
				{ }

				Record const *record() const { return _record; }
		};


		/**
		 * path element token
		 */

		struct Scanner_policy_path_element
		{
			static bool identifier_char(char c, unsigned /* i */)
			{
				return (c != '/') && (c != 0);
			}
		};


		typedef Genode::Token<Scanner_policy_path_element> Path_element_token;


		struct Node : List<Node>, List<Node>::Element
		{
			char const *name;
			Record const *record;

			Node(char const *name, Record const *record) : name(name), record(record) { }

			Node *lookup(char const *name)
			{
				Absolute_path lookup_path(name);

				if (verbose)
					PDBG("lookup_path = %s", lookup_path.base());

				Node *parent_node = this;
				Node *child_node;

				Path_element_token t(lookup_path.base());

				while (t) {

					if (t.type() != Path_element_token::IDENT) {
							t = t.next();
							continue;
					}

					char path_element[Sysio::MAX_PATH_LEN];

					t.string(path_element, sizeof(path_element));

					if (verbose)
						PDBG("path_element = %s", path_element);

					for (child_node = parent_node->first(); child_node; child_node = child_node->next()) {
						if (verbose)
							PDBG("comparing with node %s", child_node->name);
						if (strcmp(child_node->name, path_element) == 0) {
							if (verbose)
								PDBG("found matching child node");
							parent_node = child_node;
							break;
						}
					}

					if (!child_node)
						return 0;

					t = t.next();
				}

				return parent_node;
			}


			Node *lookup_child(int index)
			{
				for (Node *child_node = first(); child_node; child_node = child_node->next(), index--) {
					if (index == 0)
						return child_node;
				}

				return 0;
			}


			size_t num_dirent()
			{
				size_t count = 0;
				for (Node *child_node = first(); child_node; child_node = child_node->next(), count++) ;
				return count;
			}

		} _root_node;


		/*
		 *  Create a Node for a tar record and insert it into the node list
		 */
		class Add_node_action
		{
			private:

				Node &_root_node;

			public:

				Add_node_action(Node &root_node) : _root_node(root_node) { }

				void operator()(Record const *record)
				{
					Absolute_path current_path(record->name());

					if (verbose)
						PDBG("current_path = %s", current_path.base());

					char path_element[Sysio::MAX_PATH_LEN];

					Path_element_token t(current_path.base());

					Node *parent_node = &_root_node;
					Node *child_node;

					while(t) {

						if (t.type() != Path_element_token::IDENT) {
								t = t.next();
								continue;
						}

						Absolute_path remaining_path(t.start());

						t.string(path_element, sizeof(path_element));

						for (child_node = parent_node->first(); child_node; child_node = child_node->next()) {
							if (strcmp(child_node->name, path_element) == 0)
								break;
						}

						if (child_node) {

							if (verbose)
								PDBG("found node for %s", path_element);

							if (remaining_path.has_single_element()) {
								/* Found a node for the record to be inserted.
								 * This is usually a directory node without
								 * record. */
								child_node->record = record;
							}
						} else {
							if (remaining_path.has_single_element()) {

								if (verbose)
									PDBG("creating node for %s", path_element);

								/*
								 * TODO: find 'path_element' in 'record->name'
								 * and use the location in the record as name
								 * pointer to save some memory
								 */
								size_t name_size = strlen(path_element) + 1;
								char *name = (char*)env()->heap()->alloc(name_size);
								strncpy(name, path_element, name_size);
								child_node = new (env()->heap()) Node(name, record);
							} else {

								if (verbose)
									PDBG("creating node without record for %s", path_element);

								/* create a directory node without record */
								size_t name_size = strlen(path_element) + 1;
								char *name = (char*)env()->heap()->alloc(name_size);
								strncpy(name, path_element, name_size);
								child_node = new (env()->heap()) Node(name, 0);
							}
							parent_node->insert(child_node);
						}

						parent_node = child_node;
						t = t.next();
					}
				}
		};


		template <typename Tar_record_action>
		void _for_each_tar_record_do(Tar_record_action tar_record_action)
		{
			/* measure size of archive in blocks */
			unsigned block_id = 0, block_cnt = _tar_size/Record::BLOCK_LEN;

			/* scan metablocks of archive */
			while (block_id < block_cnt) {

				Record *record = (Record *)(_tar_base + block_id*Record::BLOCK_LEN);

				tar_record_action(record);

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
		}


		struct Num_dirent_cache
		{
			Lock             lock;
			Node            &root_node;
			bool             valid;              /* true after first lookup */
			char             key[256];           /* key used for lookup */
			size_t           cached_num_dirent;  /* cached value */

			Num_dirent_cache(Node &root_node)
			: root_node(root_node), valid(false), cached_num_dirent(0) { }

			size_t num_dirent(char const *path)
			{
				Lock::Guard guard(lock);

				/* check for cache miss */
				if (!valid || strcmp(path, key) != 0) {
					Node *node = root_node.lookup(path);
					if (!node)
						return 0;
					strncpy(key, path, sizeof(key));
					cached_num_dirent = node->num_dirent();
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
				_root_node("", 0),
				_cached_num_dirent(_root_node)
			{
				PINF("tar archive '%s' local at %p, size is %zd",
				     _rom_name.name, _tar_base, _tar_size);

				_for_each_tar_record_do(Add_node_action(_root_node));
			}


			/*********************************
			 ** Directory-service interface **
			 *********************************/

			Dataspace_capability dataspace(char const *path)
			{
				/*
				 * Walk hardlinks until we reach a file
				 */
				Record const *record = 0;
				for (;;) {
					Node *node = _root_node.lookup(path);

					if (!node)
						return Dataspace_capability();

					record = node->record;

					if (record) {
						if (record->type() == Record::TYPE_HARDLINK) {
							path = record->linked_name();
							continue;
						}

						if (record->type() == Record::TYPE_FILE)
							break;

						PERR("TAR record \"%s\" has unsupported type %d",
						     record->name(), record->type());
					}

					PERR("TAR record \"%s\" has unsupported type %d",
					     path, Record::TYPE_DIR);

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
				if (verbose)
					PDBG("path = %s", path);

				Node const *node = 0;
				Record const *record = 0;

				/*
				 * Walk hardlinks until we reach a file
				 */
				for (;;) {
					node = _root_node.lookup(path);

					if (!node) {
						sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
						return false;
					}

					record = node->record;

					if (record) {
						if (record->type() == Record::TYPE_HARDLINK) {
							path = record->linked_name();
							continue;
						} else
							break;
					} else {
						if (verbose)
							PDBG("found a virtual directoty node");
						memset(&sysio->stat_out.st, 0, sizeof(sysio->stat_out.st));
						sysio->stat_out.st.mode   = Sysio::STAT_MODE_DIRECTORY;
						return true;
					}
				}

				/* convert TAR record modes to stat modes */
				unsigned mode = record->mode();
				switch (record->type()) {
				case Record::TYPE_FILE:     mode |= Sysio::STAT_MODE_FILE; break;
				case Record::TYPE_SYMLINK:  mode |= Sysio::STAT_MODE_SYMLINK; break;
				case Record::TYPE_DIR:      mode |= Sysio::STAT_MODE_DIRECTORY; break;

				default:
					if (verbose)
						PDBG("unhandled record type %d", record->type());
				}

				memset(&sysio->stat_out.st, 0, sizeof(sysio->stat_out.st));
				sysio->stat_out.st.mode   = mode;
				sysio->stat_out.st.size   = record->size();
				sysio->stat_out.st.uid    = record->uid();
				sysio->stat_out.st.gid    = record->gid();
				sysio->stat_out.st.inode  = (unsigned long)node;

				return true;
			}

			bool dirent(Sysio *sysio, char const *path, off_t index)
			{
				Lock::Guard guard(_lock);

				Node *node = _root_node.lookup(path);

				if (!node)
					return false;

				node = node->lookup_child(index);

				if (!node) {
					sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
					return true;
				}

				sysio->dirent_out.entry.fileno = (unsigned long)node;

				Record const *record = node->record;

				if (record) {
					switch (record->type()) {
					case 0: sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_FILE;      break;
					case 2: sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_SYMLINK;   break;
					case 5: sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_DIRECTORY; break;

					default:
						if (verbose)
							PDBG("unhandled record type %d", record->type());
					}
				}

				strncpy(sysio->dirent_out.entry.name,
						node->name,
						sizeof(sysio->dirent_out.entry.name));

				return true;
			}

			bool unlink(Sysio *, char const *) { return false; }

			bool readlink(Sysio *sysio, char const *path)
			{
				Node *node = _root_node.lookup(path);
				Record const *record = node ? node->record : 0;

				if (!record || (record->type() != Record::TYPE_SYMLINK)) {
					sysio->error.readlink = Sysio::READLINK_ERR_NO_ENTRY;
					return false;
				}

				size_t const count = min(sysio->readlink_in.bufsiz,
				                         min(sizeof(sysio->readlink_out.chunk),
				                             (size_t)100));

				memcpy(sysio->readlink_out.chunk, record->linked_name(), count);

				sysio->readlink_out.count = count;

				return true;
			}

			bool rename(Sysio *, char const *, char const *) { return false; }

			bool mkdir(Sysio *, char const *) { return false; }

			bool symlink(Sysio *, char const *) { return false; }

			size_t num_dirent(char const *path)
			{
				return _cached_num_dirent.num_dirent(path);
			}

			bool is_directory(char const *path)
			{
				Node *node = _root_node.lookup(path);

				if (!node)
					return false;

				Record const *record = node->record;

				return record ? (record->type() == Record::TYPE_DIR) : true;
			}

			char const *leaf_path(char const *path)
			{
				/*
				 * Check if path exists within the file system. If this is the
				 * case, return the whole path, which is relative to the root
				 * of this file system.
				 */
				Node *node = _root_node.lookup(path);
				return node ? path : 0;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				Lock::Guard guard(_lock);

				Node *node = _root_node.lookup(path);
				if (node)
					return new (env()->heap())
						Tar_vfs_handle(this, 0, node->record);

				sysio->error.open = Sysio::OPEN_ERR_UNACCESSIBLE;
				return 0;
			}


			/***************************
			 ** File_system interface **
			 ***************************/

			static char const *name() { return "tar"; }


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

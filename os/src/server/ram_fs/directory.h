/*
 * \brief  File-system directory node
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

/* local includes */
#include <node.h>
#include <util.h>
#include <file.h>
#include <symlink.h>

namespace File_system {

	class Directory : public Node
	{
		private:

			List<Node> _entries;
			size_t     _num_entries;

		public:

			Directory(char const *name) : _num_entries(0) { Node::name(name); }

			bool has_sub_node_unsynchronized(char const *name) const
			{
				Node *sub_node = _entries.first();
				for (; sub_node; sub_node = sub_node->next())
					if (strcmp(sub_node->name(), name) == 0)
						return true;

				return false;
			}

			void adopt_unsynchronized(Node *node)
			{
				/*
				 * XXX inc ref counter
				 */
				_entries.insert(node);
				_num_entries++;
			}

			void discard_unsynchronized(Node *node)
			{
				_entries.remove(node);
				_num_entries--;
			}

			Node *lookup_and_lock(char const *path, bool return_parent = false)
			{
				if (strcmp(path, "") == 0) {
					lock();
					return this;
				}

				if (!path || path[0] == '/')
					throw Lookup_failed();

				/* find first path delimiter */
				unsigned i = 0;
				for (; path[i] && path[i] != '/'; i++);

				/*
				 * If no path delimiter was found, we are the parent of the
				 * specified path.
				 */
				if (path[i] == 0 && return_parent) {
					lock();
					return this;
				}

				/*
				 * The offset 'i' corresponds to the end of the first path
				 * element, which can be either the end of the string or the
				 * first '/' character.
				 */

				/* try to find entry that matches the first path element */
				Node *sub_node = _entries.first();
				for (; sub_node; sub_node = sub_node->next())
					if ((strlen(sub_node->name()) == i) &&
						(strcmp(sub_node->name(), path, i) == 0))
						break;

				if (!sub_node)
					throw Lookup_failed();

				if (is_basename(path)) {

					/*
					 * Because 'path' is a basename that corresponds to an
					 * existing sub_node, we have found what we were looking
					 * for.
					 */
					sub_node->lock();
					return sub_node;
				}

				/*
				 * As 'path' contains one or more path delimiters, traverse
				 * into the sub directory names after the first path element.
				 */

				/*
				 * We cannot traverse into anything other than a directory.
				 *
				 * XXX we might follow symlinks here
				 */
				Directory *sub_dir = dynamic_cast<Directory *>(sub_node);
				if (!sub_dir)
					throw Lookup_failed();

				return sub_dir->lookup_and_lock(path + i + 1, return_parent);
			}

			Directory *lookup_and_lock_dir(char const *path)
			{
				Node *node = lookup_and_lock(path);

				Directory *dir = dynamic_cast<Directory *>(node);
				if (dir)
					return dir;

				node->unlock();
				throw Lookup_failed();
			}

			File *lookup_and_lock_file(char const *path)
			{
				Node *node = lookup_and_lock(path);

				File *file = dynamic_cast<File *>(node);
				if (file)
					return file;

				node->unlock();
				throw Lookup_failed();
			}

			Symlink *lookup_and_lock_symlink(char const *path)
			{
				Node *node = lookup_and_lock(path);

				Symlink *symlink = dynamic_cast<Symlink *>(node);
				if (symlink)
					return symlink;

				node->unlock();
				throw Lookup_failed();
			}

			/**
			 * Lookup parent directory of the specified path
			 *
			 * \throw Lookup_failed
			 */
			Directory *lookup_and_lock_parent(char const *path)
			{
				return static_cast<Directory *>(lookup_and_lock(path, true));
			}

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				if (len < sizeof(Directory_entry)) {
					PERR("read buffer too small for directory entry");
					return 0;
				}

				seek_off_t index = seek_offset / sizeof(Directory_entry);

				if (seek_offset % sizeof(Directory_entry)) {
					PERR("seek offset not alighed to sizeof(Directory_entry)");
					return 0;
				}

				/* find list element */
				Node *node = _entries.first();
				for (unsigned i = 0; i < index && node; node = node->next(), i++);

				/* index out of range */
				if (!node)
					return 0;

				Directory_entry *e = (Directory_entry *)(dst);

				if (dynamic_cast<File      *>(node)) e->type = Directory_entry::TYPE_FILE;
				if (dynamic_cast<Directory *>(node)) e->type = Directory_entry::TYPE_DIRECTORY;
				if (dynamic_cast<Symlink   *>(node)) e->type = Directory_entry::TYPE_SYMLINK;

				strncpy(e->name, node->name(), sizeof(e->name));

				return sizeof(Directory_entry);
			}

			size_t write(char const *src, size_t len, seek_off_t seek_offset)
			{
				/* writing to directory nodes is not supported */
				return 0;
			}

			size_t num_entries() const { return _num_entries; }
	};
}

#endif /* _DIRECTORY_H_ */

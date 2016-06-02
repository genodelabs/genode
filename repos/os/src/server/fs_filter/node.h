/*
 * \brief  filesystem filter/router node
 * \author Ben Larson
 * \date   2016-05-13
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <os/path.h>

/* local includes */
#include "fs_registry.h"

namespace File_system {

	class Node : public Node_base
	{
	public:
		Connection  *fs;
		Node_handle  dest;
		
		Node(Connection *fs, Node_handle dest): fs(fs), dest(dest) { }
		Node(): fs(0);
		
		virtual void close()
		{
			fs->close(dest);
		}
		
		virtual Status status()
		{
			return fs->status(dest);
		}
		
		virtual void control(Control ctrl)
		{
			fs->control(dest, ctrl);
		}
	};
	
	
	class Symlink : public Node
	{
	public:
		Symlink(Connection *fs, Symlink_handle dest): Node(fs, dest);
		Symlink(): fs(0);
	};
	
	
	class File : public Node
	{
	public:
		File(Connection *fs, File_handle dest): Node(fs, dest);
		File(): fs(0);
		
		virtual void truncate(file_size_t size)
		{
			fs->truncate(dest, size);
		}
	};
	
	
	class Directory : public Node
	{
	private:
		Session_component &_session;
		String<MAX_PATH_LEN> _path;

	public:
		/* TODO: add mix functionality */

		Directory(Connection *fs, Dir_handle dest, char *path, Session_component &_session): Node(fs, dest), _path(path), _session(session) { }
		Directory(): fs(0);

		File *file(Name const &name, Mode mode, bool create)
		{
			for (Attachment *atch = _session._attachments.first(); atch; atch = atch.next() {
				if (_path == atch->dir && strcmp(name.string(), atch->name) == 0) {
					Connection target_fs = FS_reference::get_fs(atch->target_fs);
					Dir_handle dir = target_fs->dir(Name(atch->target_path), create);
					File_handle file = target_fs->file(dir, name, mode, create);
					target_fs->close(dir);
					return new (env()->heap()) File(target_fs, file);
				}
			}
			return new (env()->heap())File(fs, fs->file(static_cast<Dir_handle>(dest), name, mode, create));
		}
                
		Symlink *symlink(Name const &name, bool create)
		{
			/* TODO: handle mixed directory */
			return fs->symlink(static_cast<Dir_handle>(dest), name, mode, create);
		}

		virtual void close()
		{
			/* TODO: handle mixed directory */
			fs->close(dest);
		}
                
		virtual Status status()
		{
			/* TODO: handle mixed directory */
			return fs->status(dest);
		}
	};
	
}

#endif

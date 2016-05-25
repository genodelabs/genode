/*
 * \brief  filesystem filter/router node
 * \author Ben Larson
 * \date   2016-05-13
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <file_system/node.h>

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
	public:
		/* TODO: add mix functionality */

		Directory(Connection *fs, Dir_handle dest): Node(fs, dest);
		Directory(): fs(0);
		
		virtual File_handle file(Name const &name, Mode mode, bool create)
		{
			return fs->file(static_cast<Dir_handle>(dest), name, mode, create);
		}

		virtual Symlink_handle symlink(Name const &name, bool create)
		{
			return fs->symlink(static_cast<Symlink_handle>(dest), name, mode, create);
		}
		
		virtual void unlink(Name const &name)
		{
			fs->unlink(dest, name);
		}
	};
	
	
	class Mixed_directory : public Directory
	{
	private:
		Session_component &_session;
	public:
		Mixed_directory(Session_component &session, Dir_handle dest): Directory(session._root_fs,dest), _session(session) { }

		File_handle file(Name const &name, Mode mode, bool create)
		{
			/* TODO: handle mixed directory */
			return fs->file(static_cast<Dir_handle>(dest), name, mode, create);
		}
		
		Symlink_handle symlink(Name const &name, bool create)
		{
			/* TODO: handle mixed directory */
			return fs->symlink(static_cast<Symlink_handle>(dest), name, mode, create);
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

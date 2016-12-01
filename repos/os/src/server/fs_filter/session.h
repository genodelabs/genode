
#ifndef _SESSION_H_
#define _SESSION_H_

#include <file_system/node_handle_registry.h>
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <util/xml_node.h>
#include <file_system/util.h>

#include "util.h"
#include "fs_reference.h"

namespace File_system {

	struct Attachment : public Genode::List<Attachment>::Element
	{
		char          name[MAX_NAME_LEN];
		char          dir[MAX_PATH_LEN]; /* virtual path */
		FS_reference *target_fs;
		char          target_path[MAX_PATH_LEN]; /* "actual" path */
		bool          writeable = false; /* default to read-only */

		/* not directly from xml */
		bool          is_dir;
		char          target_parent[MAX_PATH_LEN];
// 		char          target_name[MAX_PATH_LEN];
		Dir_handle    parent_handle;
		Node_handle   target_handle;

		Attachment(Genode::Xml_node node)
		{
			node.attribute("name").value(name,sizeof(name));
			node.attribute("dir").value(dir,sizeof(dir));
			char target_fs_name[64];
			node.attribute("target_fs").value(target_fs_name,sizeof(target_fs_name));
			target_fs = FS_reference::get_fs(target_fs_name);
			node.attribute("target_path").value(target_path,sizeof(target_path));
			remove_trailing_slash(dir);
			remove_trailing_slash(target_path);
			writeable = node.attribute("writeable").has_value("yes");

			Genode::strncpy(target_parent, target_path, sizeof(target_parent));
			*const_cast<char *>(basename(target_parent)) = 0; /* terminate string before last element */

			parent_handle = target_fs->dir(target_parent, false);
			target_handle = target_fs->node(target_path);
			is_dir = target_fs->status(target_handle).directory();
		}

		~Attachment()
		{
			target_fs->close(parent_handle);
			target_fs->close(target_handle);
		}
	};
	
	class Session_component : public Session_rpc_object, public Genode::List<Session_component>::Element
	{
		friend class Directory;
	private:

		Genode::Env             &_env;
		Genode::Entrypoint      &_ep;
		FS_reference            *_root_fs;
		Directory               *_root_dir = 0;
		Node_handle_registry     _handle_registry;
		bool                     _writable = false; /* default to read-only */
		Genode::List<Attachment> _attachments;
		Genode::Allocator_avl   &_alloc;
		//List<FS_reference>   &_filesystems;

		Genode::Signal_handler<Session_component> _process_packet_dispatcher;

		void _process_packets();
		void _update_policy(Genode::Session_policy policy);
		Node *_lookup_path(Path const &path, bool dir=false, bool create=false);

	public:
		Session_component(Genode::size_t tx_buf_size, Genode::Env &env, Genode::Allocator_avl &alloc, Genode::Session_policy policy);
		~Session_component();

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create);
		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create);
		Dir_handle dir(Path const &path, bool create);
		Node_handle node(Path const &path);

		void close(Node_handle handle);
		Status status(Node_handle handle);
		void control(Node_handle handle, Control ctrl);

		void unlink(Dir_handle dir_handle, Name const &name);
		void truncate(File_handle file_handle, file_size_t size);
		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle to_dir_handle,   Name const &to_name);
		
		void sigh(Node_handle node_handle, Genode::Signal_context_capability sigh);
	};
}

#endif

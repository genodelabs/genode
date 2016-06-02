
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

#include "util.h"

namespace File_system {

	struct Attachment : public List<Attachment>::Element
	{
		char name[MAX_NAME_LEN];
		char dir[MAX_PATH_LEN];
		char target_fs[64];
		char target_path[MAX_PATH_LEN];

		Attachment(Xml_node node): {
			node.attribute("name").value(name,sizeof(name));
			node.attribute("dir").value(dir,sizeof(dir));
			node.attribute("target_fs").value(target_fs,sizeof(target_fs));
			node.attribute("target_path").value(target_path,sizeof(target_path));
			remove_trailing_slash(dir);
			remove_trailing_slash(target_path);
		}
	}
	
	class Session_component : public Session_rpc_object, public List<Session_component>::Element
	{
		friend class Directory;
	private:

		Server::Entrypoint   &_ep;
		Connection           *_root_fs;
		Directory            &_root_dir;
		Node_handle_registry  _handle_registry;
		bool                  _writable = false; /* default to read-only */
		List<Attachment>      _attachments;
		//List<FS_reference>   &_filesystems;

		Signal_rpc_member<Session_component> _process_packet_dispatcher;

		void _process_packets(unsigned)
		{
			while (tx_sink()->packet_avail() && (!tx_sink()->ready_to_ack())) {
				Packet_descriptor packet = tx_sink()->peek_packet();
				Node *node = _handle_registry.lookup(packet.handle());
				if (!node) {
					tx_sink()->get_packet(); /* dequeue */
					continue;
				}
				Session::Tx::Source &source = *node->fs->tx();
				if (!source->ready_to_submit()) return;
				/* TODO: send packet and prepare to pass acknowledgement back to client */
			}
		}
		
		void _update_policy(Session_policy policy)
		{
			try {
				_writable = policy.attribute("writeable").has_value("yes"); /* use spelling consistent with other filesystem servers in config */
			} catch (Xml_node::Nonexistent_attribute) {
				_writable = false;
				char root_fs[64];
			}
			try {
				char root_path[256];
				
				policy.attribute("root_fs").value(root_fs, sizeof(root_fs));
				_root_fs = FS_reference::get_fs(root_fs);
				
				policy.attribute("root_path").value(root_path, sizeof(root_path));

				/*
				* Make sure the root path is specified with a
				* leading path delimiter. For performing the
				* lookup, we skip the first character.
				*/
				if (root[0] != '/')
					_root_dir = Directory(); /* make invalid if update fails */
					throw Lookup_failed();

				_root_dir = Directory(_root_fs, _root_fs->dir(root_path), this);
				
				/* clear attached nodes list */
				for (Attachment atch; atch = _attachments.first(); ) {
				_attachments.remove(atch);
				destroy(env()->heap(), src);
				}
				/* update attached nodes list */
				try {
					for (Xml_node attach_node = config()->xml_node().sub_node("attach");;
						attach_node = attach_node.next("attach")) {
						_attachments.insert(Attachment(attach_node));
					}
				} catch (Xml_node::Nonexistent_sub_node) { }
				
			} catch (Xml_node::Nonexistent_attribute) {
				PERR("Missing \"root\" attribute in policy definition");
				_root_dir = Directory(); /* make invalid if update fails */
				throw Root::Unavailable();
			} catch (Lookup_failed) {
				PERR("Session root directory \"%s\" does not exist", root);
				_root_dir = Directory(); /* make invalid if update fails */
				throw Root::Unavailable();
			}
		}

	public:
		Session_component(size_t tx_buf_size, Server::Entrypoint &ep,
				Session_policy policy)
		:
			Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
			_ep(ep),
			_process_packet_dispatcher(ep, *this, &Session_component::_process_packets)
		{
			_update_policy(policy);
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}

		~Session_component()
		{
			Dataspace_capability ds = tx_sink()->dataspace();
			env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
		}

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();
			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			if (!_writable)
				if (mode != STAT_ONLY && mode != READ_ONLY)
					throw Permission_denied();

			if (create && !_writable)
				throw Permission_denied();

			File *file = dir->file(name, mode, create);
			//Node_lock_guard file_guard(file);
			return _handle_registry.alloc(file);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();
			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			if (!_writable)
				if (mode != STAT_ONLY && mode != READ_ONLY)
					throw Permission_denied();

			if (create && !_writable)
				throw Permission_denied();

			Symlink *link = dir->symlink(name, mode, create);
			//Node_lock_guard file_guard(file);
			return _handle_registry.alloc(file);
		}
		
		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();
			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			if (!_writable)
				if (mode != STAT_ONLY && mode != READ_ONLY)
					throw Permission_denied();

			if (create && !_writable)
				throw Permission_denied();

			Symlink *link = dir->symlink(name, mode, create);
			//Node_lock_guard file_guard(file);
			return _handle_registry.alloc(file);
		}

		Dir_handle dir(Path const &path, bool create)
		{
			/* TODO: add dir code */
		}

		Node_handle node(Path const &path)
		{
			/* TODO: empty stub */
		}

		void close(Node_handle handle)
		{
			Node *node = _handle_registry.lookup(handle);
			_handle_registry.free(handle);
			destroy(env()->heap(), node);
		}
		
		Status status(Node_handle node_handle)
		{
			/* TODO: empty stub */
		}
		
		void control(Node_handle, Control)
		{
			/* TODO: empty stub */
		}

		void unlink(Dir_handle dir_handle, Name const &name)
		{
			/* TODO: empty stub */
		}
		
		void truncate(File_handle file_handle, file_size_t size)
		{
			/* TODO: empty stub */
		}

		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle to_dir_handle,   Name const &to_name)
		{
			/* TODO: empty stub */
		}
		
		void sigh(Node_handle node_handle, Signal_context_capability sigh)
		{
			/* TODO: empty stub */
		}
	};
}

#endif

#include <base/stdint.h>

#include "session.h"
#include "node.h"

using namespace File_system;
using namespace Genode;

void Session_component::_process_packets()
{
	/* ready_to_ack() returns true if it ISN'T ready to acknowledge */
	while (tx_sink()->packet_avail() && (!tx_sink()->ready_to_ack())) {
		Packet_descriptor packet = tx_sink()->peek_packet();
		Node *node = _handle_registry.lookup(packet.handle());
		if (!node) {
			tx_sink()->get_packet(); /* dequeue */
			continue;
		}
		if (!node->ready_to_submit()) return;

		node->send_packet(packet);
	}
}

void Session_component::_update_policy(Session_policy policy)
{
	char root_fs[64];
	char root_path[256];

	try {
		_writable = policy.attribute("writeable").has_value("yes"); /* use spelling consistent with other filesystem servers in config */
	} catch (Xml_node::Nonexistent_attribute) {
		_writable = false;
	}
	try {
		policy.attribute("root_fs").value(root_fs, sizeof(root_fs));
		_root_fs = FS_reference::get_fs(root_fs);

		policy.attribute("root_path").value(root_path, sizeof(root_path));

		/*
		* Make sure the root path is specified with a
		* leading path delimiter. For performing the
		* lookup, we skip the first character.
		*/
		if (root_path[0] != '/')
// 			_root_dir = Directory(); /* make invalid if update fails */
			throw Lookup_failed();

		if (_root_dir)
			destroy(_alloc, _root_dir);

		_root_dir = new (_alloc) Directory(_root_fs, _root_fs->dir(Path(root_path), false), *this, _writable, "/");

		/* clear attached nodes list */
		for (Attachment *atch; (atch = _attachments.first()); ) {
			_attachments.remove(atch);
			destroy(_alloc, atch);
		}
		/* update attached nodes list */
		try {
			for (Xml_node attach_node = config()->xml_node().sub_node("attach");;
				attach_node = attach_node.next("attach")) {
				_attachments.insert(new (_alloc) Attachment(attach_node));
			}
		} catch (Xml_node::Nonexistent_sub_node) { }

	} catch (Xml_node::Nonexistent_attribute) {
		PERR("Missing \"root\" attribute in policy definition");
// 		_root_dir = Directory(); /* make invalid if update fails */
		throw Root::Unavailable();
	} catch (Lookup_failed) {
		PERR("Session root directory \"%s\" does not exist", root_path);
// 		_root_dir = Directory(); /* make invalid if update fails */
		throw Root::Unavailable();
	}
}

Node *Session_component::_lookup_path(Path const &path, bool dir, bool create)
{
	/* TODO: simplify */
	size_t longest_len = 0;
	Attachment *longest_atch = 0;
	for (Attachment *atch = _attachments.first(); atch; atch->next()) {
		size_t len = subpath(atch->target_path, path.string());
		if (len>longest_len) {
			longest_len = len;
			longest_atch = atch;
		}
	}
	if (longest_len) {
		Attachment *atch = longest_atch;
		/* .../path/to/node */
		char path_buf[MAX_PATH_LEN];
		size_t offset = 0;
		strncpy(path_buf, atch->target_path, sizeof(path_buf));
		offset = strlen(path_buf);
		strncpy(path_buf+offset, path.string()+longest_len, sizeof(path_buf)-offset);
		if (dir)
			return new (_alloc) Directory(atch->target_fs, atch->target_fs->dir(Path(path_buf), create), *this, atch->writeable, path.string());
		else
			return new (_alloc) Node(atch->target_fs, atch->target_fs->node(Path(path_buf)), *this);
	} else {
		if (dir)
			return new (_alloc) Directory(_root_fs, _root_fs->dir(path, create), *this, _writable, path.string());
		else
			return new (_alloc) Node(_root_fs, _root_fs->node(path), *this);
	}
}

Session_component::Session_component(size_t tx_buf_size, Env &env, Allocator_avl &alloc,
                  Session_policy policy):
	Session_rpc_object(env.ram().alloc(tx_buf_size), env.ep().rpc_ep()),
	_env(env),
	_ep(env.ep()),
	_alloc(alloc),
	_process_packet_dispatcher(_ep, *this, &Session_component::_process_packets)
{
	_update_policy(policy);
	_tx.sigh_packet_avail(_process_packet_dispatcher);
	_tx.sigh_ready_to_ack(_process_packet_dispatcher);
}

Session_component::~Session_component()
{
	Dataspace_capability ds = tx_sink()->dataspace();
	_env.ram().free(static_cap_cast<Ram_dataspace>(ds));
}

File_handle Session_component::file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
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
	return _handle_registry.alloc(file);
}

Symlink_handle Session_component::symlink(Dir_handle dir_handle, Name const &name, bool create)
{
	if (!valid_name(name.string()))
		throw Invalid_name();
	if (create && !_writable)
		throw Permission_denied();

	Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
	Node_lock_guard dir_guard(dir);

	Symlink *link = dir->symlink(name, create);
	return _handle_registry.alloc(link);
}

Dir_handle Session_component::dir(Path const &path, bool create)
{
	if (!valid_path(path.string()))
		throw Lookup_failed();
	if (create && !_writable)
		throw Permission_denied();

	Directory *dir = dynamic_cast<Directory*>(_lookup_path(path, true, create));
	return _handle_registry.alloc(dir);
}

Node_handle Session_component::node(Path const &path)
{
	if (!valid_path(path.string()))
		throw Lookup_failed();

	Node *node = _lookup_path(path);
	return _handle_registry.alloc(node);
}

void Session_component::close(Node_handle handle)
{
	Node *node = _handle_registry.lookup(handle);
	_handle_registry.free(handle);
	node->close();
	destroy(env()->heap(), node);
}

Status Session_component::status(Node_handle handle)
{
	Node *node = _handle_registry.lookup_and_lock(handle);
	Node_lock_guard guard(node);

	return node->status();
}

void Session_component::control(Node_handle handle, Control ctrl)
{
	Node *node = _handle_registry.lookup_and_lock(handle);
	Node_lock_guard guard(node);

	node->control(ctrl);
}

void Session_component::unlink(Dir_handle dir_handle, Name const &name)
{
	if (!valid_name(name.string()))
		throw Invalid_name();
	if (!_writable)
		throw Permission_denied();

	Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
	Node_lock_guard dir_guard(dir);

	dir->unlink(name);
}

void Session_component::truncate(File_handle file_handle, file_size_t size)
{
	File *file = _handle_registry.lookup_and_lock(file_handle);
	Node_lock_guard guard(file);

	file->truncate(size);
}

void Session_component::move(Dir_handle from_dir_handle, Name const &from_name,
			Dir_handle to_dir_handle,   Name const &to_name)
{
	Directory *from_dir = _handle_registry.lookup_and_lock(from_dir_handle);
	Node_lock_guard from_guard(from_dir);
	Directory *to_dir = _handle_registry.lookup_and_lock(to_dir_handle);
	Node_lock_guard to_guard(to_dir);

	from_dir->move(from_name, *to_dir, to_name);
}

void Session_component::sigh(Node_handle node_handle, Signal_context_capability sigh)
{
	Node *node = _handle_registry.lookup_and_lock(node_handle);
	Node_lock_guard guard(node);

	node->sigh(sigh);
}

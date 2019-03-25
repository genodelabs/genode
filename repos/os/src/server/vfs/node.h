/*
 * \brief  Internal nodes of VFS server
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2016-03-29
 */

/*
 * Copyright (C) 2016-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS__NODE_H_
#define _VFS__NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <vfs/file_system.h>
#include <os/path.h>
#include <base/id_space.h>

/* Local includes */
#include "assert.h"

namespace Vfs_server {

	using namespace File_system;
	using namespace Vfs;

	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Read_result Read_result;
	typedef Vfs::File_io_service::Sync_result Sync_result;

	typedef ::File_system::Session::Tx::Sink Packet_stream;

	class Node;
	class Io_node;
	class Watch_node;
	class Directory;
	class File;
	class Symlink;

	typedef Genode::Id_space<Node> Node_space;
	typedef Genode::Fifo<Node>     Node_queue;

	/* Vfs::MAX_PATH is shorter than File_system::MAX_PATH */
	enum { MAX_PATH_LEN = Vfs::MAX_PATH_LEN };

	typedef Genode::Path<MAX_PATH_LEN> Path;

	typedef Genode::Allocator::Out_of_memory Out_of_memory;

	/**
	 * Type trait for determining the node type for a given handle type
	 */
	template<typename T> struct Node_type;
	template<> struct Node_type<Node_handle>    { typedef Io_node    Type; };
	template<> struct Node_type<Dir_handle>     { typedef Directory  Type; };
	template<> struct Node_type<File_handle>    { typedef File       Type; };
	template<> struct Node_type<Symlink_handle> { typedef Symlink    Type; };
	template<> struct Node_type<Watch_handle>   { typedef Watch_node Type; };

	/**
	 * Type trait for determining the handle type for a given node type
	 */
	template<typename T> struct Handle_type;
	template<> struct Handle_type<Io_node>   { typedef Node_handle    Type; };
	template<> struct Handle_type<Directory> { typedef Dir_handle     Type; };
	template<> struct Handle_type<File>      { typedef File_handle    Type; };
	template<> struct Handle_type<Symlink>   { typedef Symlink_handle Type; };
	template<> struct Handle_type<Watch>     { typedef Watch_handle   Type; };

	/*
	 * Note that the file objects are created at the
	 * VFS in the local node constructors, this is to
	 * ensure that in the case of file creating that the
	 * Out_of_ram exception is thrown before the VFS is
	 * modified.
	 */
}


class Vfs_server::Node : public  ::File_system::Node_base,
                         private Node_space::Element,
                         private Node_queue::Element
{
	private:

		/*
		 * Noncopyable
		 */
		Node(Node const &);
		Node &operator = (Node const &);

		Path const _path;

	protected:

		/*
		 * Global queue of nodes that await
		 * some response from the VFS libray
		 *
		 * A global collection is perhaps dangerous
		 * but ensures fairness across sessions
		 */
		Node_queue    &_response_queue;

		/* stream used for reply packets */
		Packet_stream &_stream;

	public:

		friend Node_queue;
		using Node_queue::Element::enqueued;

		Node(Node_space &space,
		     char const *node_path,
		     Node_queue &response_queue,
		     Packet_stream &stream)
		: Node_space::Element(*this, space),
		  _path(node_path),
		  _response_queue(response_queue),
		  _stream(stream)
		{ }

		virtual ~Node()
		{
			if (enqueued())
				_response_queue.remove(*this);
		}

		using Node_space::Element::id;

		char const *path() const { return _path.base(); }

		/**
		 * Process pending activity, called by post-signal hook
		 *
		 * Default implementation is to return true so that the
		 * node is removed from the pending handle queue.
		 */
		virtual bool process_io() { return true; }

		/**
		 * Print for debugging
		 */
		void print(Genode::Output &out) const {
			out.out_string(_path.base()); }
};


/**
 * Super-class for nodes that process read/write packets
 */
class Vfs_server::Io_node : public Vfs_server::Node,
                            public Vfs::Io_response_handler{
	public:

		enum Op_state { IDLE, READ_QUEUED, SYNC_QUEUED };

	private:

		/*
		 * Noncopyable
		 */
		Io_node(Io_node const &);
		Io_node &operator = (Io_node const &);

		Mode const _mode;

		bool _packet_queued = false;
		bool _packet_op_pending = false;

	protected:

		Vfs::Vfs_handle &_handle;

		/**
		 * Packets that have been removed from the
		 * packet stream are transfered here
		 */
		Packet_descriptor _packet { };

		/**
		 * Abstract read implementation
		 *
		 * Returns true if the pending packet
		 * shall be returned to client
		 */
		bool _vfs_read(char *dst, file_size count,
		               file_offset seek_offset, file_size &out_count)
		{
			if (!(_mode & READ_ONLY)) return true;

			_handle.seek(seek_offset);

			if (!_packet_op_pending) {
				/* if the read cannot be queued with the VFS then stop here */
				if (!_handle.fs().queue_read(&_handle, count)) {
					return false;
				}
				_packet_op_pending = true;
			}

			Read_result result = _handle.fs().complete_read(
				&_handle, dst, count, out_count);

			switch (result) {
			case Read_result::READ_OK:
				_packet.succeeded(true);
				break;

			case Read_result::READ_ERR_IO:
			case Read_result::READ_ERR_INVALID:
				_packet.length(out_count);
				break;

			case Read_result::READ_ERR_WOULD_BLOCK:
			case Read_result::READ_ERR_AGAIN:
			case Read_result::READ_ERR_INTERRUPT:
			case Read_result::READ_QUEUED:
				/* packet is still pending */
				return false;
			}

			/* packet is processed */
			_packet_op_pending = false;
			return true;
		}

		/**
		 * Abstract write implementation
		 *
		 * Returns true if the pending packet
		 * shall be returned to client
		 */
		bool _vfs_write(char const *src, file_size count,
		                file_offset seek_offset, file_size &out_count)
		{
			if (!(_mode & WRITE_ONLY))
				return true;

			_handle.seek(seek_offset);

			try {
				Write_result result = _handle.fs().write(
					&_handle, src, count, out_count);

				if (result == Write_result::WRITE_OK) {
					mark_as_updated();
					_packet.succeeded(true);
				}
			}
			catch (Vfs::File_io_service::Insufficient_buffer)
			{
				/* packet is pending */
				return false;
			}

			/* packet is processed */
			return true;

			/* No further error handling! */
		}

		inline
		void _drop_packet()
		{
			_packet = Packet_descriptor();
			_packet_queued = false;
		}

		inline
		void _ack_packet(size_t count)
		{
			_packet.length(count);
			_stream.acknowledge_packet(_packet);
			_packet = Packet_descriptor();
			_packet_queued = false;
		}

		/**
		 * Abstract sync implementation
		 */
		bool _sync()
		{
			if (!_packet_op_pending) {
				/* if the sync cannot be queued with the VFS then stop here */
				if (!_handle.fs().queue_sync(&_handle)) {
					return false;
				}
				_packet_op_pending = true;
			}

			Sync_result result = _handle.fs().complete_sync(&_handle);

			switch (result) {
			case Sync_result::SYNC_OK:
				_packet.succeeded(true);
				break;

			case Sync_result::SYNC_ERR_INVALID:
				break;

			case Sync_result::SYNC_QUEUED:
				/* packet still pending */
				return false;
			}

			/* packet processed */
			_ack_packet(0);
			_packet_op_pending = false;
			return true;
		}

		/**
		 * Virtual methods for specialized node-type I/O
		 */
		virtual bool  _read() = 0;
		virtual bool _write() = 0;

	public:

		Io_node(Node_space &space, char const *node_path, Mode node_mode,
		        Node_queue &response_queue, Packet_stream &stream,
		        Vfs_handle &handle)
		: Node(space, node_path, response_queue, stream),
		  _mode(node_mode), _handle(handle)
		{
			_handle.handler(this);
		}

		virtual ~Io_node()
		{
			_handle.handler(nullptr);
			_handle.close();
		}

		using Node_space::Element::id;

		/**
		 * Process the packet that is queued at this handle
		 *
		 * Return true if the node was processed and is now idle.
		 */
		bool process_io() override
		{
			if (!_packet_queued) return true;
			if (!_stream.ready_to_ack())
				return false;

			bool result = true;

			switch (_packet.operation()) {
			case Packet_descriptor::READ:  result =  _read(); break;
			case Packet_descriptor::WRITE: result = _write(); break;
			case Packet_descriptor::SYNC:  result =  _sync(); break;

			case Packet_descriptor::READ_READY:
				/*
				 * the read-ready pending state is managed
				 * by the VFS, this packet can be discarded
				 */
				_drop_packet();

				if (_handle.fs().read_ready(&_handle)) {
					/* if the handle is ready, send a packet back immediately */
					read_ready_response();
				} else {
					/* register to send READ_READY later */
					_handle.fs().notify_read_ready(&_handle);
				}

				break;

			case Packet_descriptor::CONTENT_CHANGED:
				/* discard this packet */
				_drop_packet();
				break;
			}

			return result;
		}

		/**
		 * Process a packet by queuing it locally or sending
		 * an immediate response. Return false if no progress
		 * can be made.
		 *
		 * Called by packet stream signal handler
		 */
		bool process_packet(Packet_descriptor const &packet)
		{
			/* attempt to clear any pending packet */
			if (!process_io())
				return false;

			/* otherwise store the packet locally and process */
			_packet = packet;
			_packet_queued = true;
			process_io();
			return true;
		}

		Mode mode() const { return _mode; }


		/****************************************
		 ** Vfs::Io_response_handler interface **
		 ****************************************/

		/**
		 * Called by the VFS plugin of this handle
		 */
		void read_ready_response() override
		{
			if (!_stream.ready_to_ack()) {
				/* log a message to catch loops */
				Genode::warning("deferring READ_READY response");
				_handle.fs().notify_read_ready(&_handle);
				return;
			}

			/* Send packet immediately, though this could be queued */
			Packet_descriptor packet(Packet_descriptor(),
			                         Node_handle { id().value },
			                         Packet_descriptor::READ_READY,
			                         0, 0);
			packet.succeeded(true);
			_stream.acknowledge_packet(packet);
		}

		/**
		 * Called by the VFS plugin of this handle
		 */
		void io_progress_response() override
		{
			/*
			 * do not process packet immediately,
			 * queue to maintain ordering (priorities?)
		 	 */
			if (!enqueued())
				_response_queue.enqueue(*this);
		}
};


class Vfs_server::Watch_node final : public Vfs_server::Node,
                                     public Vfs::Watch_response_handler
{
	private:

		/*
		 * Noncopyable
		 */
		Watch_node(Watch_node const &);
		Watch_node &operator = (Watch_node const &);

		Vfs::Vfs_watch_handle &_watch_handle;

	public:

		Watch_node(Node_space &space,  char const *path,
		           Vfs::Vfs_watch_handle &handle,
		           Node_queue &response_queue,
		           Packet_stream &stream)
		: Node(space, path, response_queue, stream),
		  _watch_handle(handle)
		{
			_watch_handle.handler(this);
		}

		~Watch_node() {
			_watch_handle.close(); }


		/*******************************************
		 ** Vfs::Watch_response_handler interface **
		 *******************************************/

		void watch_response() override
		{
			/* send a packet immediately otherwise defer */
			if (!process_io() && !enqueued())
				_response_queue.enqueue(*this);
		}


		/********************************
		 ** Vfs_server::Node interface **
		 ********************************/

		/**
		 * Called by global I/O progress handler
		 */
		bool process_io() override
		{
			if (!_stream.ready_to_ack()) return false;

			Packet_descriptor packet(Packet_descriptor(),
			                         Node_handle { id().value },
			                         Packet_descriptor::CONTENT_CHANGED,
			                         0, 0);
			packet.succeeded(true);
			_stream.acknowledge_packet(packet);
			return true;
		}
};


struct Vfs_server::Symlink : Io_node
{
	protected:

		/********************
		 ** Node interface **
		 ********************/

		bool _read() override
		{
			if (_packet.position() != 0) {
				/* partial read is not supported */
				_ack_packet(0);
				return true;
			}

			file_size out_count = 0;
			bool result = _vfs_read(_stream.packet_content(_packet),
			                        _packet.length(), 0, out_count);
			if (result)
				_ack_packet(out_count);
			return result;
		}

		bool _write() override
		{
			if (_packet.position() != 0) {
				/* partial write is not supported */
				_ack_packet(0);
				return true;
			}

			file_size count = _packet.length();

			/*
			 * if the symlink target is too long return a short result
			 * because a competent File_system client will error on a
			 * length mismatch
			 */
			if (count > MAX_PATH_LEN) {
				_ack_packet(1);
				return true;
			}

			/* ensure symlink gets something null-terminated */
			Genode::String<MAX_PATH_LEN+1> target(Genode::Cstring(
				_stream.packet_content(_packet), count));
			size_t const target_len = target.length()-1;

			file_size out_count = 0;
			bool result = _vfs_write(target.string(), target_len, 0, out_count);

			if (result) {
				_ack_packet(out_count);
				if (out_count > 0) {
					mark_as_updated();
					notify_listeners();
				}
			}
			return result;
		}

		static
		Vfs_handle &_open(Vfs::File_system  &vfs, Genode::Allocator &alloc,
		                  char const  *link_path, bool create)
		{
			Vfs_handle *h = nullptr;
			assert_openlink(vfs.openlink(link_path, create, &h, alloc));
			return *h;
		}

	public:

		Symlink(Node_space        &space,
		        Vfs::File_system  &vfs,
		        Genode::Allocator &alloc,
		        Node_queue        &response_queue,
		        Packet_stream     &stream,
		        char       const  *link_path,
		        Mode               mode,
		        bool               create)
		: Io_node(space, link_path, mode, response_queue, stream,
		          _open(vfs, alloc, link_path, create))
		{ }
};


class Vfs_server::File : public Io_node
{
	private:

		/*
		 * Noncopyable
		 */
		File(File const &);
		File &operator = (File const &);

		char const *_leaf_path = nullptr; /* offset pointer to Node::_path */

		inline
		seek_off_t seek_tail(file_size count)
		{
			typedef Directory_service::Stat_result Result;
			Vfs::Directory_service::Stat st;

			/* if stat fails, try and see if the VFS will seek to the end */
			return (_handle.ds().stat(_leaf_path, st) == Result::STAT_OK)
				? ((count < st.size) ? (st.size - count) : 0)
				: (seek_off_t)SEEK_TAIL;
		}

	protected:

		bool _read() override
		{
			file_size out_count = 0;
			file_size count = _packet.length();
			seek_off_t seek_offset = _packet.position();

			if (seek_offset == (seek_off_t)SEEK_TAIL)
				seek_offset = seek_tail(count);

			bool result = _vfs_read(_stream.packet_content(_packet),
			                        count, seek_offset, out_count);
			if (result)
				_ack_packet(out_count);
			return result;
		}

		bool _write() override
		{
			file_size out_count = 0;
			file_size count = _packet.length();
			seek_off_t seek_offset = _packet.position();

			if (seek_offset == (seek_off_t)SEEK_TAIL)
				seek_offset = seek_tail(count);

			bool result = _vfs_write(_stream.packet_content(_packet),
			                         count, seek_offset, out_count);
			if (result) {
				_ack_packet(out_count);
				if (out_count > 0) {
					mark_as_updated();
					notify_listeners();
				}
			}
			return result;
		}

		static
		Vfs_handle &_open(Vfs::File_system  &vfs, Genode::Allocator &alloc,
		                  char const *file_path, Mode fs_mode, bool create)
		{
			Vfs_handle *h = nullptr;
			unsigned vfs_mode = (fs_mode-1) |
				(create ? Vfs::Directory_service::OPEN_MODE_CREATE : 0);

			assert_open(vfs.open(file_path, vfs_mode, &h, alloc));
			return *h;
		}

	public:

		File(Node_space        &space,
		     Vfs::File_system  &vfs,
		     Genode::Allocator &alloc,
		     Node_queue        &response_queue,
		     Packet_stream     &stream,
		     char       const  *file_path,
		     Mode               fs_mode,
		     bool               create)
		:
			Io_node(space, file_path, fs_mode, response_queue, stream,
			        _open(vfs, alloc, file_path, fs_mode, create))
		{
			_leaf_path = vfs.leaf_path(path());
		}

		void truncate(file_size_t size)
		{
			assert_truncate(_handle.fs().ftruncate(&_handle, size));
			mark_as_updated();
		}
};


struct Vfs_server::Directory : Io_node
{
	protected:

		/********************
		 ** Node interface **
		 ********************/

		bool _read() override
		{
			if (_packet.length() < sizeof(Directory_entry)) {
				_ack_packet(0);
				return true;
			}

			seek_off_t seek_offset = _packet.position();

			Directory_service::Dirent vfs_dirent;
			size_t blocksize = sizeof(::File_system::Directory_entry);

			unsigned index = (seek_offset / blocksize);

			file_size out_count = 0;
			bool result = _vfs_read(
				(char*)&vfs_dirent, sizeof(vfs_dirent),
				index * sizeof(vfs_dirent), out_count);

			if (result) {
				if (out_count != sizeof(vfs_dirent)) {
					_ack_packet(0);
					return true;
				}

				::File_system::Directory_entry *fs_dirent =
					(Directory_entry *)_stream.packet_content(_packet);
				fs_dirent->inode = vfs_dirent.fileno;

				switch (vfs_dirent.type) {
				case Vfs::Directory_service::DIRENT_TYPE_END:
					_ack_packet(0);
					return true;

				case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY:
					fs_dirent->type = ::File_system::Directory_entry::TYPE_DIRECTORY;
					break;
				case Vfs::Directory_service::DIRENT_TYPE_SYMLINK:
					fs_dirent->type = ::File_system::Directory_entry::TYPE_SYMLINK;
					break;
				case Vfs::Directory_service::DIRENT_TYPE_FILE:
				default:
					fs_dirent->type = ::File_system::Directory_entry::TYPE_FILE;
					break;
				}

				strncpy(fs_dirent->name, vfs_dirent.name, MAX_NAME_LEN);

				_ack_packet(sizeof(Directory_entry));
				return true;
			}
			return false;
		}

		bool _write() override
		{
			_ack_packet(0);
			return true;
		}

		static
		Vfs_handle &_open(Vfs::File_system &vfs, Genode::Allocator &alloc,
		                  char const *dir_path, bool create)
		{
			Vfs_handle *h = nullptr;
			assert_opendir(vfs.opendir(dir_path, create, &h, alloc));
			return *h;
		}

	public:

		Directory(Node_space        &space,
		          Vfs::File_system  &vfs,
		          Genode::Allocator &alloc,
		          Node_queue        &response_queue,
		          Packet_stream     &stream,
		          char const        *dir_path,
		          bool               create)
		: Io_node(space, dir_path, READ_ONLY, response_queue, stream,
		          _open(vfs, alloc, dir_path, create))
		{ }

		/**
		 * Open a file handle at this directory
		 */
		Node_space::Id file(Node_space        &space,
		                    Vfs::File_system  &vfs,
		                    Genode::Allocator &alloc,
		                    char        const *file_path,
		                    Mode               mode,
		                    bool               create)
		{
			Path subpath(file_path, path());
			char const *path_str = subpath.base();

			File *file;
			try {
				file = new (alloc) File(space, vfs, alloc,
				                        _response_queue, _stream,
				                        path_str, mode, create);
			} catch (Out_of_memory) { throw Out_of_ram(); }

			if (create)
				mark_as_updated();
			return file->id();
		}

		/**
		 * Open a symlink handle at this directory
		 */
		Node_space::Id symlink(Node_space        &space,
		                       Vfs::File_system  &vfs,
		                       Genode::Allocator &alloc,
		                       char        const *link_path,
		                       Mode               mode,
		                       bool               create)
		{
			Path subpath(link_path, path());
			char const *path_str = subpath.base();

			Symlink *link;
			try { link = new (alloc) Symlink(space, vfs, alloc,
			                                 _response_queue, _stream,
			                                 path_str, mode, create); }
			catch (Out_of_memory) { throw Out_of_ram(); }
			if (create)
				mark_as_updated();
			return link->id();
		}
};

#endif /* _VFS__NODE_H_ */

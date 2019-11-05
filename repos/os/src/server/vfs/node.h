/*
 * \brief  Internal nodes of VFS server
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \author Norman Feske
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

	struct Payload_ptr { char *ptr; };

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
	 * ensure that in the case of file creation, the
	 * Out_of_ram exception is thrown before the VFS is
	 * modified.
	 */
}


class Vfs_server::Node : Node_space::Element, Node_queue::Element
{
	private:

		/*
		 * Noncopyable
		 */
		Node(Node const &);
		Node &operator = (Node const &);

		Path const _path;

	protected:

		/**
		 * Packet descriptor to be added to the acknowledgement queue
		 *
		 * The '_acked_packet' is reset by 'submit_job' and assigned
		 * to a valid descriptor by 'try_execute_job'. The validity of the
		 * packet descriptor is tracked by '_acked_packet_valid'.
		 */
		Packet_descriptor _acked_packet { };

		bool _submit_accepted = false;

		bool _acked_packet_valid = false;

		void _reset_acked_packet()
		{
			_acked_packet = Packet_descriptor { };
			_acked_packet_valid = false;
		}

	public:

		friend Node_queue;
		using Node_queue::Element::enqueued;

		Node(Node_space &space, char const *node_path)
		:
			Node_space::Element(*this, space), _path(node_path)
		{ }

		virtual ~Node() { }

		using Node_space::Element::id;

		char const *path() const { return _path.base(); }

		enum class Submit_result { DENIED, ACCEPTED, STALLED };

		/**
		 * Submit job to node
		 *
		 * When called, the node is expected to be idle (neither queued in
		 * the active-nodes queue nor the finished-nodes queue).
		 *
		 * Must call '_reset_acked_packet'
		 */
		virtual Submit_result submit_job(Packet_descriptor, Payload_ptr)
		{
			return Submit_result::DENIED;
		}

		bool acknowledgement_valid() const
		{
			return _acked_packet_valid;
		}

		virtual void execute_job()
		{
			Genode::warning("Node::execute_job unexpectedly called");
		}

		/**
		 * Return packet descriptor to be returned as acknowledgement for the
		 * completion of the current job to the client
		 */
		Packet_descriptor acknowledgement_packet() const
		{
			if (!_acked_packet_valid)
				Genode::error("acknowledgement unexpectedly requested from node ", *this);

			return _acked_packet;
		}

		/**
		 * Print for debugging
		 */
		void print(Genode::Output &out) const
		{
			Genode::print(out, _path.string(), " (id=", id(), ")");
		}
};


/**
 * Super-class for nodes that process read/write packets
 */
class Vfs_server::Io_node : public Vfs_server::Node,
                            public Vfs::Io_response_handler
{
	private:

		/*
		 * Noncopyable
		 */
		Io_node(Io_node const &);
		Io_node &operator = (Io_node const &);

		Mode const _mode;

	protected:

		Payload_ptr _payload_ptr { };

		Vfs::Vfs_handle &_handle;

		void _import_job(Packet_descriptor packet, Payload_ptr payload_ptr)
		{
			if (enqueued())
				Genode::error("job unexpectedly submitted to busy node");

			_packet = packet;
			_payload_ptr = payload_ptr;
			_reset_acked_packet();
		}

		void _acknowledge_as_success(size_t count)
		{
			_acked_packet = _packet;
			_acked_packet.length(count);
			_acked_packet.succeeded(true);
			_acked_packet_valid = true;
		}

		void _acknowledge_as_failure()
		{
			_acked_packet = _packet;
			_acked_packet.succeeded(false);
			_acked_packet_valid = true;
		}

		/**
		 * Current job of this node, assigned by 'submit_job'
		 */
		Packet_descriptor _packet { };

		Submit_result _submit_read_at(file_offset seek_offset)
		{
			if (!(_mode & READ_ONLY))
				return Submit_result::DENIED;

			_handle.seek(seek_offset);

			return _handle.fs().queue_read(&_handle, _packet.length())
			       ? Submit_result::ACCEPTED : Submit_result::STALLED;
		}

		Submit_result _submit_write_at(file_offset seek_offset)
		{
			if (!(_mode & WRITE_ONLY))
				return Submit_result::DENIED;

			_handle.seek(seek_offset);

			return Submit_result::ACCEPTED;
		}

		Submit_result _submit_sync()
		{
			return _handle.fs().queue_sync(&_handle)
			       ? Submit_result::ACCEPTED : Submit_result::STALLED;
		}

		Submit_result _submit_read_ready()
		{
			if (_handle.fs().read_ready(&_handle)) {
				/* if the handle is ready, send a packet back immediately */
				read_ready_response();
			} else {
				/* register to send READ_READY acknowledgement later */
				_handle.fs().notify_read_ready(&_handle);
			}
			return Submit_result::ACCEPTED;
		}

		Submit_result _submit_content_changed()
		{
			Genode::warning("client unexpectedly submitted CONTENT_CHANGED packet");
			return Submit_result::DENIED;
		}

		Submit_result _submit_write_timestamp()
		{
			if (!(_mode & WRITE_ONLY))
				return Submit_result::DENIED;

			return Submit_result::ACCEPTED;
		}

		void _execute_read()
		{
			file_size out_count = 0;

			switch (_handle.fs().complete_read(&_handle, _payload_ptr.ptr,
			                                   _packet.length(), out_count)) {
			case Read_result::READ_OK:
				_acknowledge_as_success(out_count);
				break;

			case Read_result::READ_ERR_IO:
			case Read_result::READ_ERR_INVALID:
				_acknowledge_as_failure();
				break;

			case Read_result::READ_ERR_WOULD_BLOCK:
			case Read_result::READ_ERR_AGAIN:
			case Read_result::READ_ERR_INTERRUPT:
			case Read_result::READ_QUEUED:
				break;
			}
		}

		void _execute_write(char const *src_ptr, size_t length)
		{
			try {
				file_size out_count = 0;

				switch (_handle.fs().write(&_handle, src_ptr, length, out_count)) {
				case Write_result::WRITE_ERR_AGAIN:
				case Write_result::WRITE_ERR_WOULD_BLOCK:
					break;

				case Write_result::WRITE_ERR_INVALID:
				case Write_result::WRITE_ERR_IO:
				case Write_result::WRITE_ERR_INTERRUPT:
					Genode::warning("error while writing to node ", *this);
					_acknowledge_as_failure();
					break;

				case Write_result::WRITE_OK:
					_acknowledge_as_success(out_count);
					break;
				}
			}
			catch (Vfs::File_io_service::Insufficient_buffer) { /* re-execute */ }
		}

		void _execute_sync()
		{
			switch (_handle.fs().complete_sync(&_handle)) {

			case Sync_result::SYNC_OK: _acknowledge_as_success(0);
				break;

			case Sync_result::SYNC_ERR_INVALID:
				_acknowledge_as_failure();
				break;

			case Sync_result::SYNC_QUEUED:
				break;
			}
		}

		void _execute_write_timestamp()
		{
			try {
				_packet.with_timestamp([&] (File_system::Timestamp const time) {
					Vfs::Timestamp ts { .value = time.value };
					_handle.fs().update_modification_timestamp(&_handle, ts);
				});
				_acknowledge_as_success(0);
			}
			catch (Vfs::File_io_service::Insufficient_buffer) { }
		}

	public:

		Io_node(Node_space &space,
		        char const *path,
		        Mode        mode,
		        Vfs_handle &handle)
		:
			Node(space, path), _mode(mode), _handle(handle)
		{
			_handle.handler(this); // XXX remove?
		}

		virtual ~Io_node()
		{
			_handle.handler(nullptr);
			_handle.close();
		}

		using Node_space::Element::id;

		Mode mode() const { return _mode; }


		/********************************
		 ** Vfs_server::Node interface **
		 ********************************/

		void execute_job() override { }


		/****************************************
		 ** Vfs::Io_response_handler interface **
		 ****************************************/

		/**
		 * Called by the VFS plugin of this handle
		 */
		void read_ready_response() override
		{
			_acknowledge_as_success(0);
		}

		/**
		 * Called by the VFS plugin of this handle
		 */
		void io_progress_response() override { }
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

		Node_queue &_triggered_watch_nodes;

	public:

		Watch_node(Node_space            &space,
		           char            const *path,
		           Vfs::Vfs_watch_handle &handle,
		           Node_queue            &triggered_watch_nodes)
		:
			Node(space, path),
			_watch_handle(handle),
			_triggered_watch_nodes(triggered_watch_nodes)
		{
			_watch_handle.handler(this);

			/*
			 * The delivery of a watch notification is not indicated by the
			 * state of '_acked_packet_valid' but by enqueuing the node
			 * in the 'triggered_watch_nodes', which corresponds to the
			 * 'Session_component::_finished_nodes' queue.
			 */
			_acked_packet = Packet_descriptor(Packet_descriptor(),
			                                  Node_handle { id().value },
			                                  Packet_descriptor::CONTENT_CHANGED,
			                                  0, 0);
			_acked_packet.succeeded(true);
			_acked_packet_valid = true;
		}

		~Watch_node()
		{
			_watch_handle.close();
		}


		/*******************************************
		 ** Vfs::Watch_response_handler interface **
		 *******************************************/

		void watch_response() override
		{
			if (!enqueued())
				_triggered_watch_nodes.enqueue(*this);

			/*
			 * The acknowledgement and dequeuing will be delivered by
			 * 'Session_component::_try_acknowledge_jobs', which finds
			 * the watch node in the '_finished_nodes' queue.
			 */
		}


		/********************************
		 ** Vfs_server::Node interface **
		 ********************************/

		Submit_result submit_job(Packet_descriptor, Payload_ptr) override
		{
			/*
			 * This can only happen if a client misbehaves by submitting
			 * work to a watch handle.
			 */
			Genode::warning("job unexpectedly submitted to watch handle");

			/* don't reset '_acked_packet' as defined in the constructor */

			return Submit_result::DENIED;
		}
};


struct Vfs_server::Symlink : Io_node
{
	private:

		typedef Genode::String<MAX_PATH_LEN + 1> Write_buffer;

		Write_buffer _write_buffer { };

		bool _partial_operation() const
		{
			/* partial read or write is not supported */
			if (_packet.position() != 0) {
				Genode::warning("attempt for partial operation on a symlink");
				return true;
			}
			return false;
		}

		bool _max_path_length_exceeded() const
		{
			return _packet.length() >= MAX_PATH_LEN;
		}

		static Vfs_handle &_open(Vfs::File_system  &vfs, Genode::Allocator &alloc,
		                         char const *path, bool create)
		{
			Vfs_handle *h = nullptr;
			assert_openlink(vfs.openlink(path, create, &h, alloc));
			return *h;
		}

	public:

		Symlink(Node_space        &space,
		        Vfs::File_system  &vfs,
		        Genode::Allocator &alloc,
		        char        const *path,
		        Mode               mode,
		        bool               create)
		:
			Io_node(space, path, mode, _open(vfs, alloc, path, create))
		{ }

		Submit_result submit_job(Packet_descriptor packet, Payload_ptr payload_ptr) override
		{
			_import_job(packet, payload_ptr);

			switch (_packet.operation()) {

			case Packet_descriptor::READ:

				if (_partial_operation())
					return Submit_result::DENIED;

				return _submit_read_at(0);

			case Packet_descriptor::WRITE:
				{
					if (_partial_operation() || _max_path_length_exceeded())
						return Submit_result::DENIED;

					/* accessed by 'execute_job' */
					_write_buffer = Write_buffer(Genode::Cstring(_payload_ptr.ptr,
					                                             packet.length()));
					return _submit_write_at(0);
				}

			case Packet_descriptor::SYNC:            return _submit_sync();
			case Packet_descriptor::READ_READY:      return _submit_read_ready();
			case Packet_descriptor::CONTENT_CHANGED: return _submit_content_changed();
			case Packet_descriptor::WRITE_TIMESTAMP: return _submit_write_timestamp();
			}

			Genode::warning("invalid operation ", (int)_packet.operation(), " "
			                "requested from symlink node");

			return Submit_result::DENIED;
		}

		void execute_job() override
		{
			switch (_packet.operation()) {

			/*
			 * Write symlink content from '_write_buffer' instead of the
			 * '_payload_ptr'. In contrast to '_payload_ptr', which points
			 * to shared memory, the null-termination of the content of
			 * '_write_buffer' does not depend on the goodwill of the client.
			 */
			case Packet_descriptor::WRITE:
				_execute_write(_write_buffer.string(),
				               _write_buffer.length() + 1);
				break;

			/* generic */
			case Packet_descriptor::READ:            _execute_read();  break;
			case Packet_descriptor::SYNC:            _execute_sync();  break;
			case Packet_descriptor::WRITE_TIMESTAMP: _execute_write_timestamp(); break;

			/* never executed */
			case Packet_descriptor::READ_READY:
			case Packet_descriptor::CONTENT_CHANGED:
				break;
			}
		}
};


class Vfs_server::File : public Io_node
{
	private:

		/*
		 * Noncopyable
		 */
		File(File const &);
		File &operator = (File const &);

		char const * const _leaf_path = nullptr; /* offset pointer to Node::_path */

		seek_off_t _seek_pos()
		{
			seek_off_t seek_pos = _packet.position();

			if (seek_pos == (seek_off_t)SEEK_TAIL) {

				typedef Directory_service::Stat_result Result;
				Vfs::Directory_service::Stat stat { };
				if (_handle.ds().stat(_leaf_path, stat) == Result::STAT_OK)
					seek_pos = stat.size;
			}

			return seek_pos;
		}

		static Vfs_handle &_open(Vfs::File_system  &vfs, Genode::Allocator &alloc,
		                         char const *path, Mode mode, bool create)
		{
			Vfs_handle *h = nullptr;
			unsigned vfs_mode = (mode-1) |
				(create ? Vfs::Directory_service::OPEN_MODE_CREATE : 0);

			assert_open(vfs.open(path, vfs_mode, &h, alloc));
			return *h;
		}

	public:

		File(Node_space        &space,
		     Vfs::File_system  &vfs,
		     Genode::Allocator &alloc,
		     char       const  *path,
		     Mode               mode,
		     bool               create)
		:
			Io_node(space, path, mode, _open(vfs, alloc, path, mode, create)),
			_leaf_path(vfs.leaf_path(Node::path()))
		{ }

		void truncate(file_size_t size)
		{
			assert_truncate(_handle.fs().ftruncate(&_handle, size));
		}

		Submit_result submit_job(Packet_descriptor packet, Payload_ptr payload_ptr) override
		{
			_import_job(packet, payload_ptr);

			switch (_packet.operation()) {

			case Packet_descriptor::READ:            return _submit_read_at(_seek_pos());
			case Packet_descriptor::WRITE:           return _submit_write_at(_seek_pos());
			case Packet_descriptor::SYNC:            return _submit_sync();
			case Packet_descriptor::READ_READY:      return _submit_read_ready();
			case Packet_descriptor::CONTENT_CHANGED: return _submit_content_changed();
			case Packet_descriptor::WRITE_TIMESTAMP: return _submit_write_timestamp();
			}

			Genode::warning("invalid operation ", (int)_packet.operation(), " "
			                "requested from file node");

			return Submit_result::DENIED;
		}

		void execute_job() override
		{
			switch (_packet.operation()) {

			case Packet_descriptor::WRITE:
				_execute_write(_payload_ptr.ptr, _packet.length());
				break;

			/* generic */
			case Packet_descriptor::READ:            _execute_read(); break;
			case Packet_descriptor::SYNC:            _execute_sync(); break;
			case Packet_descriptor::WRITE_TIMESTAMP: _execute_write_timestamp(); break;

			/* never executed */
			case Packet_descriptor::READ_READY:
			case Packet_descriptor::CONTENT_CHANGED:
				break;
			}
		}
};


struct Vfs_server::Directory : Io_node
{
	private:

		typedef Directory_service::Dirent    Vfs_dirent;
		typedef File_system::Directory_entry Fs_dirent;

		bool _position_and_length_aligned_with_dirent_size()
		{
			if (_packet.length() < sizeof(Directory_entry))
				return false;

			if (_packet.length() % sizeof(::File_system::Directory_entry))
				return false;

			if (_packet.position() % sizeof(::File_system::Directory_entry))
				return false;

			return true;
		}

		static Fs_dirent _convert_dirent(Vfs_dirent from)
		{
			from.sanitize();

			auto fs_dirent_type = [&] (Vfs::Directory_service::Dirent_type type)
			{
				using From = Vfs::Directory_service::Dirent_type;
				using To   = ::File_system::Node_type;

				/*
				 * This should never be taken because 'END' is checked as a
				 * precondition prior the call to of this function.
				 */
				To const default_result = To::CONTINUOUS_FILE;

				switch (type) {
				case From::END:                return default_result;
				case From::DIRECTORY:          return To::DIRECTORY;
				case From::SYMLINK:            return To::SYMLINK;
				case From::CONTINUOUS_FILE:    return To::CONTINUOUS_FILE;
				case From::TRANSACTIONAL_FILE: return To::TRANSACTIONAL_FILE;
				}
				return default_result;
			};

			return {
				.inode = from.fileno,
				.type  = fs_dirent_type(from.type),
				.rwx   = {
					.readable   = from.rwx.readable,
					.writeable  = from.rwx.writeable,
					.executable = from.rwx.executable },
				.name  = { from.name.buf }
			};
		}

		/**
		 * Convert VFS directory entry to FS directory entry in place in the
		 * payload buffer
		 */
		void _convert_vfs_dirents_to_fs_dirents()
		{
			static_assert(sizeof(Vfs_dirent) == sizeof(Fs_dirent));

			file_offset const step   = sizeof(Fs_dirent);
			file_offset const length = _packet.length();

			file_size acknowledged_length = 0;

			for (file_offset offset = 0; offset + step <= length; offset += step) {

				char * const ptr = _payload_ptr.ptr + offset;

				Vfs_dirent &vfs_dirent = *(Vfs_dirent *)(ptr);
				Fs_dirent  &fs_dirent  = *(Fs_dirent  *)(ptr);

				if (vfs_dirent.type == Vfs::Directory_service::Dirent_type::END)
					break;

				fs_dirent = _convert_dirent(vfs_dirent);

				acknowledged_length += step;
			}

			/*
			 * Overwrite the acknowledgement assigned by 'Io_node::execute_job'
			 * with an acknowledgement featuring the converted length.
			 * This way, the client reads 0 bytes at the end of the directory.
			 */
			_acknowledge_as_success(acknowledged_length);
		}

		static Vfs_handle &_open(Vfs::File_system &vfs, Genode::Allocator &alloc,
		                         char const *path, bool create)
		{
			Vfs_handle *h = nullptr;
			assert_opendir(vfs.opendir(path, create, &h, alloc));
			return *h;
		}

	public:

		Directory(Node_space        &space,
		          Vfs::File_system  &vfs,
		          Genode::Allocator &alloc,
		          char const        *path,
		          bool               create)
		:
			Io_node(space, path, READ_ONLY, _open(vfs, alloc, path, create))
		{ }

		/**
		 * Open a file handle at this directory
		 */
		Node_space::Id file(Node_space        &space,
		                    Vfs::File_system  &vfs,
		                    Genode::Allocator &alloc,
		                    char        const *path,
		                    Mode               mode,
		                    bool               create)
		{
			File &file = *new (alloc)
				File(space, vfs, alloc,
				     Path(path, Node::path()).base(), mode, create);

			return file.id();
		}

		/**
		 * Open a symlink handle at this directory
		 */
		Node_space::Id symlink(Node_space        &space,
		                       Vfs::File_system  &vfs,
		                       Genode::Allocator &alloc,
		                       char        const *path,
		                       Mode               mode,
		                       bool               create)
		{
			Symlink &link = *new (alloc)
				Symlink(space, vfs, alloc,
				        Path(path, Node::path()).base(), mode, create);

			return link.id();
		}


		/********************************
		 ** Vfs_server::Node interface **
		 ********************************/

		Submit_result submit_job(Packet_descriptor packet, Payload_ptr payload_ptr) override
		{
			_import_job(packet, payload_ptr);

			switch (_packet.operation()) {

			case Packet_descriptor::READ:

				if (!_position_and_length_aligned_with_dirent_size())
					return Submit_result::DENIED;

				return _submit_read_at(_packet.position());

			case Packet_descriptor::WRITE:
				return Submit_result::DENIED;

			case Packet_descriptor::SYNC:            return _submit_sync();
			case Packet_descriptor::READ_READY:      return _submit_read_ready();
			case Packet_descriptor::CONTENT_CHANGED: return _submit_content_changed();
			case Packet_descriptor::WRITE_TIMESTAMP: return _submit_write_timestamp();
			}

			Genode::warning("invalid operation ", (int)_packet.operation(), " "
			                "requested from directory node");
			return Submit_result::DENIED;
		}

		void execute_job() override
		{
			switch (_packet.operation()) {

			case Packet_descriptor::READ:
				_execute_read();
				if (_acked_packet_valid)
					_convert_vfs_dirents_to_fs_dirents();
				break;

			/* generic */
			case Packet_descriptor::SYNC:            _execute_sync(); break;
			case Packet_descriptor::WRITE_TIMESTAMP: _execute_write_timestamp(); break;

			/* never executed */
			case Packet_descriptor::WRITE:
			case Packet_descriptor::READ_READY:
			case Packet_descriptor::CONTENT_CHANGED:
				break;
			}
		}
};

#endif /* _VFS__NODE_H_ */

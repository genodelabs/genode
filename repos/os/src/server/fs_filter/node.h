/*
 * \brief  filesystem filter/router node
 * \author Ben Larson
 * \date   2016-05-13
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <os/path.h>
#include <file_system/node.h>
#include <file_system_session/file_system_session.h>

/* local includes */
#include "fs_reference.h"
#include "session.h"

namespace File_system {

	enum {MAX_PACKET_CALLBACKS = 8};

	/* note: each Node can only be referenced by one handle */
	class Node : public Node_base
	{
	protected:
		FS_reference                 *_fs;
		Node_handle                   _dest;
		Session_component            &_session;
		bool                          _writeable=false;
		Genode::Fifo<Packet_callback> _callbacks;
		Genode::size_t                _callback_count=0;

	public:
		Node(FS_reference *fs, Node_handle dest, Session_component &session, bool writeable=false):
			_fs(fs), _dest(dest), _session(session), _writeable(writeable) { }
// 		Node(): _fs(0);
		
		bool ready_to_submit()
		{
			return _fs->tx()->ready_to_submit();
		}

		virtual void close()
		{
			_fs->close(_dest);
		}
		
		virtual ~Node()
		{
			_fs->close(_dest);
		}

		virtual Status status()
		{
			return _fs->status(_dest);
		}
		
		virtual void control(Control ctrl)
		{
			_fs->control(_dest, ctrl);
		}
		
		virtual void sigh(Genode::Signal_context_capability sigh)
		{
			_fs->sigh(_dest, sigh);
		}

		virtual bool send_packet(Packet_descriptor packet)
		{
			if (packet.operation() == Packet_descriptor::WRITE && !_writeable)
				throw Permission_denied(); /* handle cannot be used for reading or writing */

			if (_fs->tx()->ready_to_submit() && _callback_count < MAX_PACKET_CALLBACKS) {
				Packet_descriptor new_packet(_fs->tx()->alloc_packet(packet.size()), packet.handle(), packet.operation(), packet.size(), packet.position());
				Genode::memcpy(_fs->tx()->packet_content(new_packet), _session.tx_sink()->packet_content(packet), new_packet.size());
				_fs->send_packet(new_packet, *_session.tx_sink(), packet);
				return true;
			}
			return false;
		}
	};
	
	
	class Symlink : public Node
	{
	public:
		Symlink(FS_reference *fs, Symlink_handle dest, Session_component &session, bool writeable): Node(fs, dest, session, writeable) { }
// 		Symlink(): Node() { }
	};
	
	
	class File : public Node
	{
	public:
		File(FS_reference *fs, File_handle dest, Session_component &session, bool writeable): Node(fs, dest, session, writeable) { }
// 		File(): Node() { }
		
		void truncate(file_size_t size)
		{
			_fs->truncate(File_handle(_dest.value), size);
		};
	};
	
	
	class Directory : public Node
	{
	private:
// 		Session_component        &_session;
		Genode::String<MAX_PATH_LEN>      _path;
		Genode::Signal_handler<Directory> _sigh_chainer;
		Genode::Signal_context_capability _sigh;
		Genode::Allocator                &_alloc;

		struct Operation
		{
			enum LOOKUP_OP {OP_FILE, OP_SYMLINK, OP_UNLINK, OP_MOVE};

			Directory          &parent;
			LOOKUP_OP           opcode;
			Name                name;
			Mode                mode;
			bool                create;
			Directory          *new_dir;
			Name                new_name;

			Operation(Directory &parent): parent(parent) { }

			Node *execute()
			{
				Symlink_handle symlink;
				File_handle file;
				for (Attachment *atch = parent._session._attachments.first(); atch; atch = atch->next()) {
					if (parent._path == atch->dir && Genode::strcmp(name.string(), atch->name) == 0) {
						/* match found */
						FS_reference *target_fs = atch->target_fs;

						Dir_handle dir = atch->parent_handle; //target_fs->dir(Name(atch->target_path), false);
						Node_handle_guard dir_guard(target_fs, dir);

						switch(opcode) {
						case OP_FILE:
							file = target_fs->file(dir, name, mode, create);
							return new (parent._alloc) File(target_fs, file, parent._session, (mode==WRITE_ONLY || mode==READ_WRITE)? true : false);

						case OP_SYMLINK:
							symlink = target_fs->symlink(dir, name, create);
							return new (parent._alloc) Symlink(target_fs, symlink, parent._session, create);

						case OP_UNLINK:
							target_fs->unlink(dir, name);
							return 0;

						case OP_MOVE:
							if (target_fs != new_dir->_fs)
								throw Permission_denied();
							target_fs->move(dir, name, Dir_handle(new_dir->_dest.value), new_name);
							return 0;
						}
					}
					return 0; /* TODO: throw error instead of returning */
				}

				/* no attachment */
				Dir_handle dir = Dir_handle(parent._dest.value);
				switch(opcode) {
				case OP_FILE:
					file = parent._fs->file(dir, name, mode, create);
					return new (parent._alloc) File(parent._fs, file, parent._session, (mode==WRITE_ONLY || mode==READ_WRITE)? true : false);

				case OP_SYMLINK:
					symlink = parent._fs->symlink(dir, name, create);
					return new (parent._alloc) Symlink(parent._fs, symlink, parent._session, (mode==WRITE_ONLY || mode==READ_WRITE)? true : false);

				case OP_UNLINK:
					parent._fs->unlink(dir, name);
					return 0;

				case OP_MOVE:
					if (parent._fs != new_dir->_fs)
						throw Permission_denied();
					parent._fs->move(dir, name, Dir_handle(new_dir->_dest.value), new_name);
					return 0;
				}
			}

			static File *file(Directory &parent, Name const &name, Mode mode, bool create)
			{
				Operation op(parent);
				op.opcode = OP_FILE;
				op.name = name;
				op.mode = mode;
				op.create = create;
				return static_cast<File *>(op.execute());
			}

			static Symlink *symlink(Directory &parent, Name const &name, bool create)
			{
				Operation op(parent);
				op.opcode = OP_SYMLINK;
				op.name = name;
				op.create = create;
				return static_cast<Symlink *>(op.execute());
			}

			static void unlink(Directory &parent, Name const &name)
			{
				Operation op(parent);
				op.opcode = OP_UNLINK;
				op.name = name;
				op.execute();
			}

			static void move(Directory &parent, Name const &name, Directory &new_dir, Name const &new_name)
			{
				Operation op(parent);
				op.opcode = OP_MOVE;
				op.name = name;
				op.new_dir = &new_dir;
				op.new_name = new_name;
				op.execute();
			}
		};

	public:
		Directory(FS_reference *fs, Dir_handle dest, Session_component &session, bool writeable, const char *path):
			Node(fs, dest, session, writeable), _path(path), _sigh_chainer(_session._ep, *this, &Directory::submit_signal), _alloc(session._alloc)
			{ }

// 		Directory(): Node();

		File *file(Name const &name, Mode mode, bool create)
		{
			return Operation::file(*this, name, mode, create);
		}
                
		Symlink *symlink(Name const &name, bool create)
		{
			return Operation::symlink(*this, name, create);
		}

		void unlink(Name const &name)
		{
			Operation::unlink(*this, name);
		}

		Status status()
		{
			Status stat = Node::status();

			unsigned atch_count = 0;
			for (Attachment *atch = _session._attachments.first(); atch; atch = atch->next())
				if (_path == atch->dir) ++atch_count;

			stat.size += atch_count * sizeof(Directory_entry);
			return stat;
		}

		void sigh(Genode::Signal_context_capability sigh)
		{
			_sigh = sigh;
			Node::sigh(_sigh_chainer);
			for (Attachment *atch = _session._attachments.first(); atch; atch = atch->next())
				if (_path == atch->dir) atch->target_fs->sigh(atch->target_handle, _sigh);
		}

		void submit_signal()
		{
			if (_sigh.valid()) Genode::Signal_transmitter(_sigh).submit();
		}

		void move(Name const &name, Directory &new_dir, Name const &new_name)
		{
			Operation::move(*this, name, new_dir, new_name);
		}

		bool send_packet(Packet_descriptor packet)
		{
			/* TODO: handle mixed directory */
			return Node::send_packet(packet);
		};
	};
	
}

#endif

/*
 * \brief  Block device file system
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2013-12-20
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_
#define _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_

#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <vfs/single_file_system.h>

namespace Vfs { class Block_file_system; }


class Vfs::Block_file_system : public Single_file_system
{
	private:

		struct Label
		{
			enum { LABEL_MAX_LEN = 64 };
			char string[LABEL_MAX_LEN];

			Label(Xml_node config)
			{
				string[0] = 0;
				try { config.attribute("label").value(string, sizeof(string)); }
				catch (...) { }
			}
		} _label;

		/*
		 * Serialize access to packet stream of the block session
		 */
		Lock _lock;

		char                       *_block_buffer;
		unsigned                    _block_buffer_count;

		Genode::Allocator_avl       _tx_block_alloc;
		Block::Connection           _block;
		Genode::size_t              _block_size;
		Block::sector_t             _block_count;
		Block::Session::Operations  _block_ops;
		Block::Session::Tx::Source *_tx_source;

		bool                        _readable;
		bool                        _writeable;

		file_size _block_io(file_size nr, void *buf, file_size sz,
		                    bool write, bool bulk = false)
		{
			Lock::Guard guard(_lock);

			Block::Packet_descriptor::Opcode op;
			op = write ? Block::Packet_descriptor::WRITE : Block::Packet_descriptor::READ;

			file_size packet_size  = bulk ? sz : _block_size;
			file_size packet_count = bulk ? (sz / _block_size) : 1;

			/* sanity check */
			if (packet_count > _block_buffer_count) {
				packet_size  = _block_buffer_count * _block_size;
				packet_count = _block_buffer_count;
			}

			Block::Packet_descriptor p(_tx_source->alloc_packet(packet_size), op,
			                           nr, packet_count);

			if (write)
				Genode::memcpy(_tx_source->packet_content(p), buf, packet_size);

			_tx_source->submit_packet(p);
			p = _tx_source->get_acked_packet();

			if (!p.succeeded()) {
				Genode::error("Could not read block(s)");
				_tx_source->release_packet(p);
				return 0;
			}

			if (!write)
				Genode::memcpy(buf, _tx_source->packet_content(p), packet_size);

			_tx_source->release_packet(p);
			return packet_size;
		}

	public:

		Block_file_system(Xml_node config)
		:
			Single_file_system(NODE_TYPE_BLOCK_DEVICE, name(), config),
			_label(config),
			_block_buffer(0),
			_block_buffer_count(1),
			_tx_block_alloc(env()->heap()),
			_block(&_tx_block_alloc, 128*1024, _label.string),
			_tx_source(_block.tx()),
			_readable(false),
			_writeable(false)
		{
			try { config.attribute("block_buffer_count").value(&_block_buffer_count); }
			catch (...) { }

			_block.info(&_block_count, &_block_size, &_block_ops);

			_readable  = _block_ops.supported(Block::Packet_descriptor::READ);
			_writeable = _block_ops.supported(Block::Packet_descriptor::WRITE);

			_block_buffer = new (env()->heap()) char[_block_buffer_count * _block_size];
		}

		~Block_file_system()
		{
			destroy(env()->heap(), _block_buffer);
		}

		static char const *name() { return "block"; }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result const result = Single_file_system::stat(path, out);
			out.size = _block_count * _block_size;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *vfs_handle, char const *buf,
		                   file_size count, file_size &out_count) override
		{
			if (!_writeable) {
				Genode::error("block device is not writeable");
				return WRITE_ERR_INVALID;
			}

			file_size seek_offset = vfs_handle->seek();

			file_size written = 0;
			while (count > 0) {
				file_size displ   = 0;
				file_size length  = 0;
				file_size nbytes  = 0;
				file_size blk_nr  = seek_offset / _block_size;

				displ = seek_offset % _block_size;

				if ((displ + count) > _block_size)
					length = (_block_size - displ);
				else
					length = count;

				/*
				 * We take a shortcut and write as much as possible without
				 * using the block buffer if the offset is aligned on a block
				 * boundary and the count is a multiple of the block size,
				 * e.g. 4K writes will be written at once.
				 *
				 * XXX this is quite hackish because we have to omit partial
				 * blocks at the end.
				 */
				if (displ == 0 && (count % _block_size) >= 0 && !(count < _block_size)) {
					file_size bytes_left = count - (count % _block_size);

					nbytes = _block_io(blk_nr, (void*)(buf + written),
					                   bytes_left, true, true);
					if (nbytes == 0) {
						Genode::error("error while write block:", blk_nr, " from block device");
						return WRITE_ERR_INVALID;
					}

					written += nbytes;
					count   -= nbytes;
					seek_offset += nbytes;

					continue;
				}

				/*
				 * The offset is not aligned on a block boundary. Therefore
				 * we need to read the block to the block buffer first and
				 * put the buffer content at the right offset before we can
				 * write the whole block back. In addition if length is less
				 * than block size, we also have to read the block first.
				 */
				if (displ > 0 || length < _block_size) {

					_block_io(blk_nr, _block_buffer, _block_size, false);
					/* rewind seek offset to account for the block read */
					seek_offset -= _block_size;
				}

				Genode::memcpy(_block_buffer + displ, buf + written, length);

				nbytes = _block_io(blk_nr, _block_buffer, _block_size, true);
				if ((unsigned)nbytes != _block_size) {
					Genode::error("error while writing block:", blk_nr, " to Block_device");
					return WRITE_ERR_INVALID;
				}

				written += length;
				count -= length;
				seek_offset += length;
			}

			out_count = written;

			return WRITE_OK;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                 file_size &out_count) override
		{
			if (!_readable) {
				Genode::error("block device is not readable");
				return READ_ERR_INVALID;
			}

			file_size seek_offset = vfs_handle->seek();

			file_size read = 0;
			while (count > 0) {
				file_size displ   = 0;
				file_size length  = 0;
				file_size nbytes  = 0;
				file_size blk_nr  = seek_offset / _block_size;

				displ = seek_offset % _block_size;

				if ((displ + count) > _block_size)
					length = (_block_size - displ);
				else
					length = count;

				/*
				 * We take a shortcut and read the blocks all at once if t he
				 * offset is aligned on a block boundary and we the count is a
				 * multiple of the block size, e.g. 4K reads will be read at
				 * once.
				 *
				 * XXX this is quite hackish because we have to omit partial
				 * blocks at the end.
				 */
				if (displ == 0 && (count % _block_size) >= 0 && !(count < _block_size)) {
					file_size bytes_left = count - (count % _block_size);

					nbytes = _block_io(blk_nr, dst + read, bytes_left, false, true);
					if (nbytes == 0) {
						Genode::error("error while reading block:", blk_nr, " from block device");
						return READ_ERR_INVALID;
					}

					read  += nbytes;
					count -= nbytes;
					seek_offset += nbytes;

					continue;
				}

				nbytes = _block_io(blk_nr, _block_buffer, _block_size, false);
				if ((unsigned)nbytes != _block_size) {
					Genode::error("error while reading block:", blk_nr, " from block device");
					return READ_ERR_INVALID;
				}

				Genode::memcpy(dst + read, _block_buffer + displ, length);

				read  += length;
				count -= length;
				seek_offset += length;
			}

			out_count = read;

			return READ_OK;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			return FTRUNCATE_OK;
		}

		Ioctl_result ioctl(Vfs_handle *vfs_handle, Ioctl_opcode opcode, Ioctl_arg,
		                   Ioctl_out &out) override
		{
			switch (opcode) {
			case IOCTL_OP_DIOCGMEDIASIZE:

				out.diocgmediasize.size = _block_count * _block_size;
				return IOCTL_OK;

			default:

				Genode::warning("invalid ioctl request ", (int)opcode);
				break;
			}

			/* never reached */
			return IOCTL_ERR_INVALID;
		}
};

#endif /* _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_ */

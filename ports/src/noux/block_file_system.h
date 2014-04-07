/*
 * \brief  Block device file system
 * \author Josef Soentgen
 * \date   2013-12-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__BLOCK_FILE_SYSTEM_H_
#define _NOUX__BLOCK_FILE_SYSTEM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/lock.h>
#include <base/stdint.h>
#include <block_session/connection.h>
#include <util/string.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include "file_system.h"


namespace Noux {

	class Block_file_system : public File_system
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

			char                       *_block_buffer;
			unsigned                    _block_buffer_count;

			Genode::Allocator_avl       _tx_block_alloc;
			Block::Connection           _block;
			size_t                      _block_size;
			Block::sector_t             _block_count;
			Block::Session::Operations  _block_ops;
			Block::Session::Tx::Source *_tx_source;

			off_t                       _seek_offset;

			bool                        _readable;
			bool                        _writeable;


			enum { FILENAME_MAX_LEN = 64 };
			char _filename[FILENAME_MAX_LEN];

			bool _is_root(const char *path)
			{
				return (strcmp(path, "") == 0) || (strcmp(path, "/") == 0);
			}

			bool _is_block_file(const char *path)
			{
				return (strlen(path) == (strlen(_filename) + 1)) &&
				       (strcmp(&path[1], _filename) == 0);
			}

			size_t _block_io(size_t nr, void *buf, size_t sz, bool write, bool bulk = false)
			{
				Block::Packet_descriptor::Opcode op;
				op = write ? Block::Packet_descriptor::WRITE : Block::Packet_descriptor::READ;

				size_t packet_size  = bulk ? sz : _block_size;
				size_t packet_count = bulk ? (sz / _block_size) : 1;

				/* sanity check */
				if (packet_count > _block_buffer_count) {
					packet_size  = _block_buffer_count * _block_size;
					packet_count = _block_buffer_count;
				}

				//if (verbose)
					PDBG("%5s: block:%zu size:%zu packets:%zu",
					     write ? "write" : "read", nr, sz, packet_count);

				Block::Packet_descriptor p(_tx_source->alloc_packet(packet_size), op,
				                           nr, packet_count);

				if (write)
					Genode::memcpy(_tx_source->packet_content(p), buf, packet_size);

				_tx_source->submit_packet(p);
				p = _tx_source->get_acked_packet();

				if (!p.succeeded()) {
					PERR("Could not read block(s)");
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
				_label(config),
				_block_buffer(0),
				_block_buffer_count(1),
				_tx_block_alloc(env()->heap()),
				_block(&_tx_block_alloc, 128*1024, _label.string),
				_tx_source(_block.tx()),
				_seek_offset(0),
				_readable(false),
				_writeable(false)
			{
				_filename[0] = 0;
				try { config.attribute("name").value(_filename, sizeof(_filename)); }
				catch (...) { }

				try { config.attribute("block_buffer_count").value(&_block_buffer_count); }
				catch (...) { }

				_block.info(&_block_count, &_block_size, &_block_ops);

				_readable  = _block_ops.supported(Block::Packet_descriptor::READ);
				_writeable = _block_ops.supported(Block::Packet_descriptor::WRITE);

				_block_buffer = new (env()->heap()) char[_block_buffer_count * _block_size];

				//if (verbose)
					PDBG("number of blocks: %llu with block size: %zu (bytes)"
					     " , readable: %d writeable: %d",
					     _block_count, _block_size, _readable, _writeable);
			}

			~Block_file_system()
			{
				destroy(env()->heap(), _block_buffer);
			}


			/*********************************
			 ** Directory-service interface **
			 *********************************/

			Dataspace_capability dataspace(char const *path)
			{
				/* not supported */
				return Dataspace_capability();
			}

			void release(char const *path, Dataspace_capability ds_cap)
			{
				/* not supported */
			}

			bool stat(Sysio *sysio, char const *path)
			{
				Sysio::Stat st = { 0, 0, 0, 0, 0, 0 };

				if (_is_root(path))
					st.mode = Sysio::STAT_MODE_DIRECTORY;
				else if (_is_block_file(path)) {
					st.mode = Sysio::STAT_MODE_BLOCKDEV;
				} else {
					sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
					return false;
				}

				sysio->stat_out.st = st;
				return true;
			}

			bool dirent(Sysio *sysio, char const *path, off_t index)
			{
				if (_is_root(path)) {
					if (index == 0) {
						sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_BLOCKDEV;
						strncpy(sysio->dirent_out.entry.name,
						        _filename,
						        sizeof(sysio->dirent_out.entry.name));
					} else {
						sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
					}

					return true;
				}

				return false;
			}

			size_t num_dirent(char const *path)
			{
				if (_is_root(path))
					return 1;
				else
					return 0;
			}

			bool is_directory(char const *path)
			{
				if (_is_root(path))
					return true;

				return false;
			}

			char const *leaf_path(char const *path)
			{
				return path;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				if (!_is_block_file(path)) {
					sysio->error.open = Sysio::OPEN_ERR_UNACCESSIBLE;
					return 0;
				}

				return new (env()->heap()) Vfs_handle(this, this, 0);
			}

			bool unlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool readlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool rename(Sysio *sysio, char const *from_path, char const *to_path)
			{
				/* not supported */
				return false;
			}

			bool mkdir(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool symlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			/***************************
			 ** File_system interface **
			 ***************************/

			static char const *name() { return "block"; }


			/********************************
			 ** File I/O service interface **
			 ********************************/

			bool write(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				if (!_writeable) {
					PERR("block device is not writeable");
					return false;
				}

				size_t      _seek_offset = vfs_handle->seek();
				size_t      count = sysio->write_in.count;
				const char  *_buf = (const char*) sysio->write_in.chunk;

				size_t written = 0;
				while (count > 0) {
					size_t displ   = 0;
					size_t length  = 0;
					size_t nbytes  = 0;
					size_t blk_nr  = _seek_offset / _block_size;

					displ = _seek_offset % _block_size;

					if ((displ + count) > _block_size)
						length = (_block_size - displ);
					else
						length = count;

					/**
					 * We take a shortcut and write as much as possible without
					 * using the block buffer if the offset is aligned on a block
					 * boundary and the count is a multiple of the block size,
					 * e.g. 4K writes will be written at once.
					 *
					 * XXX this is quite hackish because we have to omit partial
					 * blocks at the end.
					 */
					if (displ == 0 && (count % _block_size) >= 0 && !(count < _block_size)) {
						size_t bytes_left = count - (count % _block_size);

						nbytes = _block_io(blk_nr, (void*)(_buf + written),
						                   bytes_left, true, true);
						if (nbytes == 0) {
							PERR("error while reading block:%zu from block device",
							     blk_nr);
							return false;
						}

						written += nbytes;
						count   -= nbytes;

						continue;
					}

					/**
					 * The offset is not aligned on a block boundary. Therefore
					 * we need to read the block to the block buffer first and
					 * put the buffer content at the right offset before we can
					 * write the whole block back. In addition if length is less
					 * than block size, we also have to read the block first.
					 */
					if (displ > 0 || length < _block_size) {
						PWRN("offset:%zd block_size:%zd displacement:%zd length:%zu",
								_seek_offset, _block_size, displ, length);

						_block_io(blk_nr, _block_buffer, _block_size, false);
						/* rewind seek offset to account for the block read */
						_seek_offset -= _block_size;
					}

					Genode::memcpy(_block_buffer + displ, _buf + written, length);

					nbytes = _block_io(blk_nr, _block_buffer, _block_size, true);
					if ((unsigned)nbytes != _block_size) {
						PERR("error while reading block:%zu from Block_device",
						     blk_nr);
						return false;
					}

					written += length;
					count -= length;
				}

				sysio->write_out.count = written;

				return true;
			}

			bool read(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				if (!_readable) {
					PERR("block device is not readable");
					return false;
				}

				size_t _seek_offset = vfs_handle->seek();
				size_t count      = sysio->read_in.count;
				char  *_buf       = (char*) sysio->read_out.chunk;

				size_t read = 0;
				while (count > 0) {
					size_t displ   = 0;
					size_t length  = 0;
					size_t nbytes  = 0;
					size_t blk_nr  = _seek_offset / _block_size;

					displ = _seek_offset % _block_size;

					if ((displ + count) > _block_size)
						length = (_block_size - displ);
					else
						length = count;

					/**
					 * We take a shortcut and read the blocks all at once if t he
					 * offset is aligned on a block boundary and we the count is a
					 * multiple of the block size, e.g. 4K reads will be read at
					 * once.
					 *
					 * XXX this is quite hackish because we have to omit partial
					 * blocks at the end.
					 */
					if (displ == 0 && (count % _block_size) >= 0 && !(count < _block_size)) {
						size_t bytes_left = count - (count % _block_size);

						nbytes = _block_io(blk_nr, _buf + read, bytes_left, false, true);
						if (nbytes == 0) {
							PERR("error while reading block:%zu from block device",
							     blk_nr);
							return false;
						}

						read += nbytes;
						count -= nbytes;

						continue;
					}

					if (displ > 0)
						PWRN("offset:%zd is not aligned to block_size:%zu"
						     " displacement:%zu", _seek_offset, _block_size,
						     displ);

					nbytes = _block_io(blk_nr, _block_buffer, _block_size, false);
					if ((unsigned)nbytes != _block_size) {
						PERR("error while reading block:%zu from block device",
						     blk_nr);
						return false;
					}

					Genode::memcpy(_buf + read, _block_buffer + displ, length);

					read += length;
					count -= length;
				}

				sysio->read_out.count = read;

				return true;
			}

			bool ftruncate(Sysio *sysio, Vfs_handle *vfs_handle) { return true; }

			bool check_unblock(Vfs_handle *vfs_handle, bool rd, bool wr, bool ex)
			{
				return true;
			}

			bool ioctl(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				switch (sysio->ioctl_in.request) {
				case Sysio::Ioctl_in::OP_DIOCGMEDIASIZE:
					{
						sysio->ioctl_out.diocgmediasize.size = _block_count * _block_size;
						return true;
					}
				default:
					PDBG("invalid ioctl request %d", sysio->ioctl_in.request);
					return false;
				}

				/* never reached */
				return false;
			}
	};
}

#endif /* _NOUX__BLOCK_FILE_SYSTEM_H_ */

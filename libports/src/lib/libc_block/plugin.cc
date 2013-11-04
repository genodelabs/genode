/*
 * \brief  Libc Block_session plugin
 * \author Josef Soentgen
 * \date   2013-11-04
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <block_session/connection.h>

/* libc plugin includes */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <sys/disk.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

using namespace Genode;


static bool const verbose = false;

/**
 * There is a off_t typedef clash between sys/socket.h
 * and base/stdint.h. We define the macro here to circumvent
 * this issue.
 */
#undef DIOCGMEDIASIZE
#define DIOCGMEDIASIZE _IOR('d', 129, Genode::off_t)

/************
 ** Plugin **
 ************/

namespace {

	struct Plugin_context : Libc::Plugin_context
	{
		private:

			Genode::Allocator          *_md_alloc;
			Genode::Allocator_avl       _tx_block_alloc;

			char                       *_block_buffer;

			Block::Connection          *_block;
			size_t                      _block_size;
			size_t                      _block_count;
			Block::Session::Operations  _block_ops;
			Block::Session::Tx::Source *_tx_source;

			::off_t                     _cur_offset;

			bool                        _readable;
			bool                        _writeable;

			int                         _flags;

		public:


			enum { BUFFER_BLOCK_NUMBER = 16 };

			Plugin_context(Genode::Allocator *md_alloc, int flags)
			:
				_md_alloc(md_alloc),
				_tx_block_alloc(_md_alloc),
				_block_buffer(0),
				_block(new (_md_alloc) Block::Connection(&_tx_block_alloc)),
				_tx_source(_block->tx()),
				_cur_offset(0),
				_readable(false),
				_writeable(false),
				_flags(flags)
			{
				_block->info(&_block_count, &_block_size, &_block_ops);

				_readable  = _block_ops.supported(Block::Packet_descriptor::READ);
				_writeable = _block_ops.supported(Block::Packet_descriptor::WRITE);

				_block_buffer = reinterpret_cast<char*>(malloc(BUFFER_BLOCK_NUMBER * _block_size));

				if (verbose) {
					PDBG("number of blocks: %zu with block size: %zu (bytes)"
					     " , readable: %d writeable: %d",
					     _block_count, _block_size, _readable, _writeable);
				}
			}

			~Plugin_context()
			{
				destroy(_md_alloc, _block);
				free(_block_buffer);
			}

			/**
			 * Return read/write state
			 */
			bool readable()  const { return _readable; }
			bool writeable() const { return _writeable; }

			/**
			 * Return file descriptor flags
			 */
			int  flags()     const { return _flags; }

			/**
			 * Return seek offset
			 */
			::off_t seek_offset() const { return _cur_offset; }

			/**
			 * Set seek offset
			 */
			void seek_offset(size_t seek_offset) { _cur_offset = seek_offset; }

			/**
			 * Advance current seek offset position by 'incr' number of bytes
			 */
			void advance_seek_offset(size_t incr) { _cur_offset += incr; }

			/**
			 * Set seek offset to maximum value
			 */
			void infinite_seek_offset() { _cur_offset = ~0; }

			/**
			 * Return pointer to memory used as block puffer
			 */
			char *block_buffer() const { return _block_buffer; }

			/**
			 * Return number of blocks
			 */
			size_t block_count() const { return _block_count; }

			/**
			 * Return size of a block
			 */
			size_t block_size() const { return _block_size; }

			/**
			 * Issue a block operation
			 *
			 * This implementation operates synchronized, it will wait for the
			 * acknowlegdement for the submitted packet.
			 *
			 * \param nr number of start block
			 * \param buf buffer to read from or write to
			 * \param sz size of buffer
			 * \param write if true issue write operation, otherwise issue read opertion
			 * \param bulk if true operate on as many block as fit into the block buffer
			 *
			 * return number of bytes transfered
			 */
			ssize_t block_io(size_t nr, void *buf, size_t sz, bool write, bool bulk = false)
			{
				/* sync offset */
				_cur_offset = nr * _block_size;

				Block::Packet_descriptor::Opcode op;
				op = write ? Block::Packet_descriptor::WRITE : Block::Packet_descriptor::READ;

				size_t packet_size  = bulk ? sz : _block_size;
				size_t packet_count = bulk ? (sz / _block_size) : 1;

				/* sanity check */
				if (packet_count > BUFFER_BLOCK_NUMBER) {
					packet_size  = BUFFER_BLOCK_NUMBER * _block_size;
					packet_count = BUFFER_BLOCK_NUMBER;
				}

				if (verbose)
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
					errno = EIO;
					return -1;
				}

				if (!write)
					Genode::memcpy(buf, _tx_source->packet_content(p), packet_size);

				_cur_offset += packet_size;

				_tx_source->release_packet(p);
				return packet_size;
			}

	};

	static inline Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}

	class Plugin : public Libc::Plugin
	{
		private:

			/**
			 * File name this plugin feels responsible for
			 */
			static char const *_dev_name() { return "/dev/blkdev"; }

			bool _supports_path(const char *path)
			{
				return (Genode::strcmp(path, "/dev") == 0) ||
				       (Genode::strcmp(path, _dev_name()) == 0);
			}

		public:

			/**
			 * Constructor
			 */
			Plugin() { }

			bool supports_open(const char *pathname, int flags) {
				return _supports_path(pathname); }

			bool supports_stat(const char *path) {
				return _supports_path(path); }

			int close(Libc::File_descriptor *fd)
			{
				Plugin_context *ctx = context(fd);

				destroy(env()->heap(), ctx);
				Libc::file_descriptor_allocator()->free(fd);

				return 0;
			}

			int fcntl(Libc::File_descriptor *fd, int cmd, long arg)
			{
				switch (cmd) {
				case F_GETFL:
					return context(fd)->flags();
				case F_SETLK:
					/**
					 * We do not support locking a block device but we keep
					 * the caller happy.
					 */
					return 0;
				default:
					PDBG("cmd: %d not implemented, return error.", cmd);
					return -1;
				}

				return -1; /* never reached */
			}

			int fstat(Libc::File_descriptor *fd, struct stat *buf)
			{
				/* we pretent to be a block device */
				if (buf) {
					Genode::memset(buf, 0, sizeof (struct stat));
					buf->st_mode = S_IFBLK;
				}

				return 0;
			}

			int fsync(Libc::File_descriptor *fd)
			{
				/**
				 * Currently all block I/O is done synchronized. To
				 * keep the caller happy we pretent that the fsync()
				 * call was successful.
				 */
				return 0;
			}

			int ioctl(Libc::File_descriptor *fd, int req, char *argp)
			{
				Plugin_context *ctx = context(fd);

				switch (req) {
				case DIOCGMEDIASIZE:
					return ctx->block_count();
				case DIOCGSECTORSIZE:
					return ctx->block_size();
				default:
					PDBG("request: %d not supported", req);
					return -1;
				}
			}

			::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
			{
				Plugin_context *ctx = context(fd);

				switch (whence) {
				case SEEK_SET:
					ctx->seek_offset(offset);
					return offset;

				case SEEK_CUR:
					ctx->advance_seek_offset(offset);
					return ctx->seek_offset();

				case SEEK_END:
					if (offset != 0) {
						errno = EINVAL;
						return -1;
					}
					ctx->infinite_seek_offset();
					return (ctx->block_count() * ctx->block_size());

				default:
					errno = EINVAL;
					return -1;
				}
			}

			Libc::File_descriptor *open(const char *pathname, int flags)
			{
				try {
					if (verbose)
						PDBG("open block device '%s'", pathname);

					Plugin_context *context = new (Genode::env()->heap())
						Plugin_context(env()->heap(), flags);

					return Libc::file_descriptor_allocator()->alloc(this, context);
				} catch (...) {
					PERR("could not create plugin context");
				}

				errno = ENOENT;
				return 0;
			}

			ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
			{
				Plugin_context *ctx = context(fd);

				if (!ctx->readable()) {
					PERR("block device is not readable");
					errno = EINVAL;
					return -1;
				}

				size_t blk_size   = ctx->block_size();
				char  *blk_buffer = ctx->block_buffer();
				char  *_buf       = (char*) buf;

				ssize_t read = 0;
				while (count > 0) {
					size_t displ   = 0;
					size_t length  = 0;
					ssize_t nbytes = 0;
					size_t blk_nr  = ctx->seek_offset() / blk_size;

					displ = ctx->seek_offset() % blk_size;

					if ((displ + count) > blk_size)
						length = (blk_size - displ);
					else
						length = count;

					/**
					 * We take a shortcut and read the blocks all at once if the
					 * offset is aligned on a block boundary and we the count is a
					 * multiple of the block size, e.g. 4K reads will be read at
					 * once.
					 *
					 * XXX this is quite hackish because we have to omit partial
					 * blocks at the end.
					 */
					if (displ == 0 && (count % blk_size) >= 0 && !(count < blk_size)) {
						size_t bytes_left = count - (count % blk_size);

						nbytes = ctx->block_io(blk_nr, _buf + read, bytes_left, false, true);
						if (nbytes == -1) {
							PERR("error while reading block:%zu from block device",
							     blk_nr);
							return -1;
						}

						read += nbytes;
						count -= nbytes;

						continue;
					}

					if (displ > 0)
						PWRN("offset:%lld is not aligned to block_size:%zu"
						     " displacement:%zu", ctx->seek_offset(), blk_size, displ);

					nbytes = ctx->block_io(blk_nr, blk_buffer, blk_size, false);
					if (nbytes != blk_size) {
						PERR("error while reading block:%zu from block device",
						     blk_nr);
						return -1;
					}

					Genode::memcpy(_buf + read, blk_buffer + displ, length);

					read += length;
					count -= length;
				}

				return read;
			}

			int stat(const char *path, struct stat *buf)
			{
				/* we pretent to be a block device */
				if (buf) {
					Genode::memset(buf, 0, sizeof (struct stat));
					if (Genode::strcmp(path, "/dev") == 0)
						buf->st_mode = S_IFDIR;
					else if (Genode::strcmp(path, _dev_name()) == 0)
						buf->st_mode = S_IFBLK;
					else {
						errno = ENOENT;
						return -1;
					}
				}
				return 0;
			}

			ssize_t write(Libc::File_descriptor *fd, const void *buf, ::size_t count)
			{
				Plugin_context *ctx = context(fd);

				if (!ctx->writeable()) {
					PERR("block device is not writeable");
					errno = EINVAL;
					return -1;
				}

				size_t blk_size   = ctx->block_size();
				char  *blk_buffer = ctx->block_buffer();
				const char  *_buf = (const char*)buf;

				ssize_t written = 0;
				while (count > 0) {
					size_t displ   = 0;
					size_t length  = 0;
					size_t nbytes  = 0;
					size_t blk_nr  = ctx->seek_offset() / blk_size;

					displ = ctx->seek_offset() % blk_size;

					if ((displ + count) > blk_size)
						length = (blk_size - displ);
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
					if (displ == 0 && (count % blk_size) >= 0 && !(count < blk_size)) {
						size_t bytes_left = count - (count % blk_size);

						nbytes = ctx->block_io(blk_nr, (void*)(_buf + written),
						                       bytes_left, true, true);
						if (nbytes == -1) {
							PERR("error while reading block:%zu from block device",
							     blk_nr);
							return -1;
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
					if (displ > 0 || length < blk_size) {
						PWRN("offset:%lld block_size:%zd displacement:%zd length:%zu",
						     ctx->seek_offset(), blk_size, displ, length);

						ctx->block_io(blk_nr, blk_buffer, blk_size, false);
						/* rewind seek offset to account for the block read */
						ctx->seek_offset(ctx->seek_offset()-blk_size);
					}

					Genode::memcpy(blk_buffer + displ, _buf + written, length);

					nbytes = ctx->block_io(blk_nr, blk_buffer, blk_size, true);
					if (nbytes != blk_size) {
						PERR("error while reading block:%zu from Block_device",
						     blk_nr);
						return -1;
					}

					written += length;
					count -= length;
				}

				return written;
			}

	};

} /* unnamed namespace */

void __attribute__((constructor)) init_libc_block(void)
{
	PDBG("using the libc_block plugin");
	static Plugin plugin;
}

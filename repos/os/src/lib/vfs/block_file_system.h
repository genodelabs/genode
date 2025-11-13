/*
 * \brief  Block device file system
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2013-12-20
 */

/*
 * Copyright (C) 2013-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_
#define _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>
#include <vfs/single_file_system.h>


namespace Vfs { class Block_file_system; }


struct Vfs::Block_file_system
{
	using Name = String<64>;
	using block_number_t = Block::block_number_t;
	using off_t = Block::off_t;

	struct Block_count { Block::block_number_t blocks; };

	struct Operation_size
	{
		Block::block_count_t blocks;

		static Operation_size from_block_count(Block_count const count) {
			return { (Block::block_count_t)count.blocks }; }
	};

	struct Block_job : Block::Connection<Block_job>::Job
	{
		/*
		 * The range covers the whole request from the client
		 * but the overall amount is limited to the block
		 * count in the operation and thus can be smaller.
		 */
		Byte_range_ptr const range;
		file_size      const start_offset;

		size_t bytes_handled { 0 };

		/* store length for acking unaligned or partial requests */
		size_t actual_length { 0 };

		bool done    { false };
		bool success { false };

		Block_job(Block::Connection<Block_job> &conn,
		           char *           const  start,
		           size_t           const  num_bytes,
		           file_size        const  start_offset,
		           Block::Operation const &op)
		:
			Block::Connection<Block_job>::Job {conn, op },
			range        { start, num_bytes },
			start_offset { start_offset }
		{ }

		size_t bytes_remaining() const {
			return range.num_bytes - bytes_handled; }
	};

	struct Block_connection : Block::Connection<Block_job>
	{
		Block_connection(auto &&... args)
		: Block::Connection<Block_job>(args...) { }

		/*****************************************************
		 ** Block::Connection::Update_jobs_policy interface **
		 *****************************************************/

		void produce_write_content(Block_job &job, off_t offset,
		                           char *dst, size_t length)
		{
			size_t sz = min(length, job.bytes_remaining());
			if (length + offset > job.range.num_bytes) {
				Genode::error("write job outside request boundary");
				return;
			}

			char const * src = (char const*)job.range.start + offset;
			memcpy(dst + job.start_offset, src, length);

			job.bytes_handled += sz;
		}

		void consume_read_result(Block_job &job, off_t offset,
		                         char const *src, size_t length)
		{
			size_t sz = min(length, job.bytes_remaining());
			if (!sz)
				return;

			char * dst = (char*)job.range.start + offset;
			memcpy(dst, src + job.start_offset, sz);

			job.bytes_handled += sz;
		}

		void completed(Block_job &job, bool success)
		{
			job.success = success;
			job.done    = true;
		}
	};

	struct Local_factory;
	struct Data_file_system;
	struct Compound_file_system;
};


class Vfs::Block_file_system::Data_file_system : public Single_file_system
{
	private:

		/*
		 * Noncopyable
		 */
		Data_file_system(Data_file_system const &);
		Data_file_system &operator = (Data_file_system const &);

		Vfs::Env &_env;

		Block_connection &_block;

		class Block_vfs_handle : public Single_vfs_handle
		{
			private:

				/*
				 * Noncopyable
				 */
				Block_vfs_handle(Block_vfs_handle const &);
				Block_vfs_handle &operator = (Block_vfs_handle const &);

				friend class Data_file_system;

				Block_connection &_block;

				struct Size_helper
				{
					size_t    const _size;
					file_size const _mask, _mask_inv;

					Size_helper(size_t const block_size)
					:
						_size     { block_size },
						_mask     { _size - 1 },
						_mask_inv { ~_mask }
					{ }

					size_t block_size() const {
						return _size; }

					file_size mask(file_size value) const {
						return value & _mask; }

					file_size round_up(file_size value) const {
						return (value + _mask) & _mask_inv; }

					file_size round_down(file_size value) const {
						return value & _mask_inv; }

					Block_count blocks(file_size value) const {
						return Block_count { value / _size }; }

					block_number_t block_number(file_size value) const {
						return block_number_t { value / _size }; }
				};

				struct Read_handler
				{
					Genode::Constructible<Block_job> _job { };

					Size_helper const _helper;
					Block_count const _block_count;

					bool _any_pending_job() const {
						return _job.constructed() && !_job->done; }

					bool _any_finished_job() const {
						return _job.constructed() && _job->done; }

					Read_result _handle_finished_job(size_t &out_count)
					{
						out_count = _job->bytes_handled;
						bool const success = _job->success;
						_job.destruct();
						return success ? Read_result::READ_OK
						               : Read_result::READ_ERR_IO;
					}

					Read_handler(Block::Session::Info const &info)
					:
						_helper       { info.block_size },
						_block_count  { info.block_count }
					{ }

					Read_result read(Block_connection            &block,
					                 file_size             const  seek_offset,
					                 Byte_range_ptr        const &dst,
					                 size_t                      &out_count)
					{
						/* fast-exit for pending jobs */
						if (_any_pending_job())
							return Read_result::READ_QUEUED;

						if (_any_finished_job())
							return _handle_finished_job(out_count);

						/* round down to cover first block for unaligned requests */
						block_number_t const block_number =
							_helper.block_number(_helper.round_down(seek_offset));

						file_size const block_offset =
							_helper.mask(seek_offset);

						/* always round up to cover last block for partial requests */
						file_size const rounded_length =
							_helper.round_up(dst.num_bytes + block_offset);

						Block_count block_count =
							_helper.blocks(rounded_length);

						if (block_number + block_count.blocks > _block_count.blocks)
							block_count = Block_count { _block_count.blocks - block_number };

						if (block_number >= _block_count.blocks || block_count.blocks == 0) {
							out_count = 0;
							return Read_result::READ_OK;
						}

						Block::Operation const op {
							.type         = Block::Operation::Type::READ,
							.block_number = block_number,
							.count        = Operation_size::from_block_count(block_count).blocks
						};

						/*
						 * The Job API handles splitting the request and the
						 * job will read up to dst.num_bytes at most.
						 */
						_job.construct(block, dst.start, dst.num_bytes, block_offset, op);

						block.update_jobs(block);
						return Read_result::READ_QUEUED;
					}
				};

				Read_handler _read_handler;

				struct Write_handler
				{
					Genode::Constructible<Block_job> _job { };

					Size_helper const _helper;
					Block_count const _block_count;

					char _unaligned_buffer[4096] { };

					bool _any_pending_job() const {
						return _job.constructed() && !_job->done; }

					bool _any_finished_job() const {
						return _job.constructed() && _job->done; }

					Write_result _handle_unaligned_or_partial(Block_connection       &block,
					                                          block_number_t   const  block_number)
					{
						Block::Operation const op {
							.type         = Block::Operation::Type::READ,
							.block_number = block_number,
							.count        = 1
						};

						_job.construct(block, _unaligned_buffer,
						               _helper.block_size(), 0, op);

						block.update_jobs(block);
						return Write_result::WRITE_ERR_WOULD_BLOCK;
					}

					Write_result _handle_finished_job(Block_connection           &block,
					                                  Const_byte_range_ptr const &src,
					                                  file_size            const  block_offset,
					                                  block_number_t       const  block_number,
					                                  size_t                     &out_count)
					{
						/*
						 * We do not care whether READ (for unaligned or partial requests)
						 * or WRITE has failed and handle this first. Then handle
						 * successful WRITE requests as we can ack them directly.
						 */

						if (!_job->success) {
							_job.destruct();
							return Write_result::WRITE_ERR_IO;
						}

						if (_job->operation().type == Block::Operation::Type::WRITE) {
							out_count = _job->actual_length ? _job->actual_length
							                                : _job->bytes_handled;
							_job.destruct();
							return Write_result::WRITE_OK;
						}

						/*
						 * Now we deal with completing a unaligned or partial request.
						 */
						file_size const partial_length =
							block_offset ? min(_helper.block_size() - block_offset, src.num_bytes)
							             : src.num_bytes;

						/* should never happen */
						if (!partial_length)
							return Write_result::WRITE_ERR_IO;

						memcpy(_unaligned_buffer + block_offset, src.start, (size_t)partial_length);

						Block::Operation const op {
							.type         = Block::Operation::Type::WRITE,
							.block_number = block_number,
							.count        = 1
						};

						_job.construct(block, _unaligned_buffer,
						               _helper.block_size(), 0, op);

						/* store partial length for setting the matching out_count later */
						_job->actual_length = (size_t)partial_length;

						block.update_jobs(block);
						return Write_result::WRITE_ERR_WOULD_BLOCK;
					}

					Write_handler(Block::Session::Info const &info)
					:
						_helper       { info.block_size },
						_block_count  { info.block_count }
					{ }

					Write_result write(Block_connection            &block,
					                   file_size             const  seek_offset,
					                   Const_byte_range_ptr  const &src,
					                   size_t                      &out_count)
					{
						/* fast-exit for pending jobs */
						if (_any_pending_job())
							return Write_result::WRITE_ERR_WOULD_BLOCK;

						file_size const block_offset =
							_helper.mask(seek_offset);

						/* round down to handle partial requests later on */
						file_size const rounded_length =
							_helper.round_down(src.num_bytes + block_offset);

						Block_count const block_count =
							_helper.blocks(rounded_length);

						block_number_t const block_number =
							_helper.block_number(_helper.round_down(seek_offset));

						if (block_number >= _block_count.blocks
						 || block_number + block_count.blocks > _block_count.blocks)
							return Write_result::WRITE_ERR_INVALID;

						/*
						 * The actual WRITE job will be set up here if the finished
						 * job was in charge of reading in the block for unaligned or
						 * partial requests.
						 */

						if (_any_finished_job())
							return _handle_finished_job(block, src, block_offset,
							                            block_number, out_count);

						/*
						 * If the request is unaligned or partial we first have to
						 * issue a READ request for the block covering the request.
						 */

						if (block_offset != 0)
							return _handle_unaligned_or_partial(block, block_number);

						if (!rounded_length && src.num_bytes < _helper.block_size())
							return _handle_unaligned_or_partial(block, block_number);

						/*
						 * Handle regular aligned, full-block request.
						 */

						Block::Operation const op {
							.type         = Block::Operation::Type::WRITE,
							.block_number = block_number,
							.count        = Operation_size::from_block_count(block_count).blocks
						};

						/*
						 * The Job API handles splitting the request and the
						 * job will write up to src.num_bytes at most.
						 */
						_job.construct(block, const_cast<char*>(src.start), src.num_bytes,
						               block_offset, op);

						block.update_jobs(block);
						return Write_result::WRITE_ERR_WOULD_BLOCK;
					}
				};

				Write_handler _write_handler;

				struct Sync_handler
				{
					Genode::Constructible<Block_job> _job {  };

					Block_count const _block_count;

					bool _any_pending_job() const {
						return _job.constructed() && !_job->done; }

					bool _any_finished_job() const {
						return _job.constructed() && _job->done; }

					Sync_result _handle_finished_job()
					{
						bool const success = _job->success;
						_job.destruct();
						return success ? Sync_result::SYNC_OK
						               : Sync_result::SYNC_ERR_INVALID;
					}

					Sync_handler(Block::Session::Info const &info)
					: _block_count { info.block_count } { }

					Sync_result sync(Block_connection &block)
					{
						if (_any_pending_job())
							return Sync_result::SYNC_QUEUED;

						if (_any_finished_job())
							return _handle_finished_job();

						Block::Operation const op {
							.type         = Block::Operation::Type::SYNC,
							.block_number = 0,
							.count        = Operation_size::from_block_count(_block_count).blocks
						};

						_job.construct(block, nullptr, 0, 0, op);

						block.update_jobs(block);
						return Sync_result::SYNC_QUEUED;
					}
				};

				Sync_handler _sync_handler;

			public:

				Block_vfs_handle(Directory_service          &ds,
				                 File_io_service            &fs,
				                 Allocator                  &alloc,
				                 Block_connection           &block)
				:
					Single_vfs_handle { ds, fs, alloc, 0 },
					_block            { block },
					_read_handler     { _block.info() },
					_write_handler    { _block.info() },
					_sync_handler     { _block.info() }
				{ }

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override {
					return _read_handler.read(_block, seek(), dst, out_count); }

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					if (!_block.info().writeable)
						return WRITE_ERR_INVALID;

					/*
					 * Since there is no explicit queue reseult value we issue
					 * WRITE_ERR_WOULD_BLOCK for this case.
					 */
					return _write_handler.write(_block, seek(), src, out_count);
				}

				Sync_result sync() override {
					return _sync_handler.sync(_block); }

				bool read_ready()  const override { return true; }

				bool write_ready() const override { return true; }
		};

	public:

		Data_file_system(Vfs::Env                &env,
		                 Block_connection        &block,
		                 Name              const &name)
		:
			Single_file_system { Node_type::CONTINUOUS_FILE, name.string(),
			                     block.info().writeable ? Node_rwx::rw()
			                                            : Node_rwx::ro(),
			                     Node() },
			_env   { env },
			_block { block }
		{
			/* prevent usage of unsupported block sizes */
			size_t const block_size = _block.info().block_size;
			if (block_size % 512 != 0 ||
			    block_size > sizeof(Block_vfs_handle::Write_handler::_unaligned_buffer)) {
				Genode::error("block-size: ", block_size, " of underlying session not supported");
				struct Unsupported_underlying_block_size { };
				throw Unsupported_underlying_block_size();
			}
		}

		~Data_file_system() { }

		static char const *name()   { return "data"; }
		char const *type() override { return "data"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc) Block_vfs_handle(*this, *this, alloc,
				                                           _block);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result const result = Single_file_system::stat(path, out);
			out.size = _block.info().block_count * _block.info().block_size;
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


struct Vfs::Block_file_system::Local_factory : File_system_factory
{
	using Label = Genode::String<64>;
	Label const _label;

	Name const _name;

	Vfs::Env &_env;

	Genode::Allocator_avl _tx_block_alloc { &_env.alloc() };

	Block_connection _block;

	Genode::Io_signal_handler<Local_factory> _block_signal_handler {
		_env.env().ep(), *this, &Local_factory::_handle_block_signal };

	void _handle_block_signal()
	{
		_block.update_jobs(_block);
		_env.user().wakeup_vfs_user();
	}

	Data_file_system _data_fs;
	
	struct Info : Block::Session::Info
	{
		void print(Genode::Output &out) const
		{
			char buf[128] { };
			Genode::Generator::generate({ buf, sizeof(buf) }, "block",
				[&] (Genode::Generator &g) {
					g.attribute("count", Block::Session::Info::block_count);
					g.attribute("size",  Block::Session::Info::block_size);
			}).with_error([] (Genode::Buffer_error) {
				Genode::warning("VFS-block info exceeds maximum buffer size");
			});
			Genode::print(out, Genode::Cstring(buf));
		}
	};

	Readonly_value_file_system<Info>             _info_fs        { "info",        Info { } };
	Readonly_value_file_system<Genode::uint64_t> _block_count_fs { "block_count", 0 };
	Readonly_value_file_system<size_t>           _block_size_fs  { "block_size",  0 };

	static Name name(Node const &config) {
		return config.attribute_value("name", Name("block")); }

	static constexpr Genode::Num_bytes DEFAULT_IO_BUFFER_SIZE = { (4u << 20) };

	/* payload size, a fixed-amount for meta-data is added below */
	static size_t io_buffer(Node const &config) {
		return config.attribute_value("io_buffer", DEFAULT_IO_BUFFER_SIZE); }

	Local_factory(Vfs::Env &env, Node const &config)
	:
		_label   { config.attribute_value("label", Label("")) },
		_name    { name(config) },
		_env     { env },
		_block   { _env.env(), &_tx_block_alloc, io_buffer(config) + (64u << 10),
		           _label.string() },
		_data_fs { _env, _block, name(config) }
	{
		if (config.has_attribute("block_buffer_count"))
			Genode::warning("'block_buffer_count' attribute is superseded by 'io_buffer'");

		_block.sigh(_block_signal_handler);
		_info_fs       .value(Info { _block.info() });
		_block_count_fs.value(_block.info().block_count);
		_block_size_fs .value(_block.info().block_size);
	}

	Vfs::File_system *create(Vfs::Env&, Node const &node) override
	{
		if (node.has_type("data"))        return &_data_fs;
		if (node.has_type("info"))        return &_info_fs;
		if (node.has_type("block_count")) return &_block_count_fs;
		if (node.has_type("block_size"))  return &_block_size_fs;
		return nullptr;
	}
};


class Vfs::Block_file_system::Compound_file_system : private Local_factory,
                                                     public  Vfs::Dir_file_system
{
	private:

		using Name = Block_file_system::Name;

		using Config = String<200>;
		static Config _config(Name const &name)
		{
			char buf[Config::capacity()] { };

			/*
			 * By not using the node type "dir", we operate the
			 * 'Dir_file_system' in root mode, allowing multiple sibling nodes
			 * to be present at the mount point.
			 */
			Genode::Generator::generate({ buf, sizeof(buf) }, "compound",
				[&] (Genode::Generator &g) {

					g.node("data", [&] { g.attribute("name", name); });

					g.node("dir", [&] {
						g.attribute("name", Name(".", name));
						g.node("info");
						g.node("block_count");
						g.node("block_size");
					});

			}).with_error([&] (Genode::Buffer_error) {
				Genode::warning("VFS-block compound exceeds maximum buffer size");
			});

			return Config(Genode::Cstring(buf));
		}

	public:

		Compound_file_system(Vfs::Env &vfs_env, Node const &node)
		:
			Local_factory { vfs_env, node },
			Vfs::Dir_file_system { vfs_env,
			                       Node(_config(Local_factory::name(node))),
			                       *this }
		{ }

		static const char *name() { return "block"; }

		char const *type() override { return name(); }
};

#endif /* _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_ */

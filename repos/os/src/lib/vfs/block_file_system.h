/*
 * \brief  Block device file system
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2013-12-20
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_
#define _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <util/xml_generator.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>
#include <vfs/single_file_system.h>


namespace Vfs { class Block_file_system; }


struct Vfs::Block_file_system
{
	typedef String<64> Name;

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

		char     *_block_buffer;
		unsigned  _block_buffer_count;

		Block::Connection<>        &_block;
		Block::Session::Info const &_info;
		Block::Session::Tx::Source *_tx_source;

		bool const _writeable;

		class Block_vfs_handle : public Single_vfs_handle
		{
			private:

				Genode::Entrypoint                &_ep;
				Genode::Allocator                 &_alloc;
				char                              *_block_buffer;
				unsigned                          &_block_buffer_count;
				Block::Connection<>               &_block;
				size_t                       const _block_size;
				Block::sector_t              const _block_count;
				Block::Session::Tx::Source        *_tx_source;
				bool                         const _writeable;

				void _block_for_ack()
				{
					while (!_tx_source->ack_avail())
						_ep.wait_and_dispatch_one_io_signal();
				}

				/*
				 * Noncopyable
				 */
				Block_vfs_handle(Block_vfs_handle const &);
				Block_vfs_handle &operator = (Block_vfs_handle const &);

				file_size _block_io(file_size nr, void *buf, file_size sz,
				                    bool write, bool bulk = false)
				{
					Block::Packet_descriptor::Opcode op;
					op = write ? Block::Packet_descriptor::WRITE : Block::Packet_descriptor::READ;

					size_t packet_count = bulk ? (size_t)(sz / _block_size) : 1;

					Block::Packet_descriptor packet;

					/* sanity check */
					if (packet_count > _block_buffer_count) {
						packet_count = _block_buffer_count;
					}

					while (true) {
						try {
							packet = _block.alloc_packet(packet_count * _block_size);
							break;
						} catch (Block::Session::Tx::Source::Packet_alloc_failed) {

							while (!_tx_source->ready_to_submit())
								_ep.wait_and_dispatch_one_io_signal();

							if (packet_count > 1) {
								packet_count /= 2;
							}
						}
					}

					Block::Packet_descriptor p(packet, op, nr, packet_count);

					if (write)
						Genode::memcpy(_tx_source->packet_content(p), buf, packet_count * _block_size);

					_tx_source->submit_packet(p);

					_block_for_ack();

					p = _tx_source->get_acked_packet();

					if (!p.succeeded()) {
						Genode::error("Could not read block(s)");
						_tx_source->release_packet(p);
						return 0;
					}

					if (!write)
						Genode::memcpy(buf, _tx_source->packet_content(p), packet_count * _block_size);

					_tx_source->release_packet(p);
					return packet_count * _block_size;
				}

			public:

				Block_vfs_handle(Directory_service                 &ds,
				                 File_io_service                   &fs,
				                 Genode::Entrypoint                &ep,
				                 Genode::Allocator                 &alloc,
				                 char                              *block_buffer,
				                 unsigned                          &block_buffer_count,
				                 Block::Connection<>               &block,
				                 size_t                             block_size,
				                 Block::sector_t                    block_count,
				                 Block::Session::Tx::Source        *tx_source,
				                 bool                               writeable)
				:
					Single_vfs_handle(ds, fs, alloc, 0),
					_ep(ep),
					_alloc(alloc),
					_block_buffer(block_buffer),
					_block_buffer_count(block_buffer_count),
					_block(block),
					_block_size(block_size),
					_block_count(block_count),
					_tx_source(tx_source),
					_writeable(writeable)
				{ }

				Read_result read(char *dst, file_size count,
				                 file_size &out_count) override
				{
					file_size seek_offset = seek();

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
						 * We take a shortcut and read the blocks all at once if the
						 * offset is aligned on a block boundary and the count is a
						 * multiple of the block size, e.g. 4K reads will be read at
						 * once.
						 *
						 * XXX this is quite hackish because we have to omit partial
						 * blocks at the end.
						 */
						if (displ == 0 && !(count < _block_size)) {
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

						Genode::memcpy(dst + read, _block_buffer + displ, (size_t)length);

						read  += length;
						count -= length;
						seek_offset += length;
					}

					out_count = read;

					return READ_OK;

				}

				Write_result write(char const *buf, file_size count,
				                   file_size &out_count) override
				{
					if (!_writeable) {
						Genode::error("block device is not writeable");
						return WRITE_ERR_INVALID;
					}

					file_size seek_offset = seek();

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
						if (displ == 0 && !(count < _block_size)) {
							file_size bytes_left = count - (count % _block_size);

							nbytes = _block_io(blk_nr, (void*)(buf + written),
							                   bytes_left, true, true);
							if (nbytes == 0) {
								Genode::error("error while write block:", blk_nr, " to block device");
								return WRITE_ERR_INVALID;
							}

							written     += nbytes;
							count       -= nbytes;
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
						if (displ > 0 || length < _block_size)
							_block_io(blk_nr, _block_buffer, _block_size, false);

						Genode::memcpy(_block_buffer + displ, buf + written, (size_t)length);

						nbytes = _block_io(blk_nr, _block_buffer, _block_size, true);
						if ((unsigned)nbytes != _block_size) {
							Genode::error("error while writing block:", blk_nr, " to block_device");
							return WRITE_ERR_INVALID;
						}

						written     += length;
						count       -= length;
						seek_offset += length;
					}

					out_count = written;

					return WRITE_OK;

				}

				Sync_result sync() override
				{
					/*
					 * Just in case bail if we cannot submit the packet.
					 * (Since the plugin operates in a synchronous fashion
					 * that should not happen.)
					 */
					if (!_tx_source->ready_to_submit()) {
						Genode::error("vfs_block: could not sync blocks");
						return SYNC_ERR_INVALID;
					}

					Block::Packet_descriptor p =
						Block::Session::sync_all_packet_descriptor(
							_block.info(), Block::Session::Tag { 0 });

					_tx_source->submit_packet(p);

					_block_for_ack();

					p = _tx_source->get_acked_packet();
					_tx_source->release_packet(p);

					if (!p.succeeded()) {
						Genode::error("vfs_block: syncing blocks failed");
						return SYNC_ERR_INVALID;
					}

					return SYNC_OK;
				}

				bool read_ready() override { return true; }
		};

	public:

		Data_file_system(Vfs::Env                   &env,
		                 Block::Connection<>        &block,
		                 Block::Session::Info const &info,
		                 Name                 const &name,
		                 unsigned                    block_buffer_count)
		:
			Single_file_system { Node_type::CONTINUOUS_FILE, name.string(),
			                     info.writeable ? Node_rwx::rw() : Node_rwx::ro(),
			                     Genode::Xml_node("<data/>") },
			_env(env),
			_block_buffer(0),
			_block_buffer_count(block_buffer_count),
			_block(block),
			_info(info),
			_tx_source(_block.tx()),
			_writeable(_info.writeable)
		{
			_block_buffer = new (_env.alloc())
				char[_block_buffer_count * _info.block_size];
		}

		~Data_file_system()
		{
			destroy(_env.alloc(), _block_buffer);
		}

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
				*out_handle = new (alloc) Block_vfs_handle(*this, *this,
				                                           _env.env().ep(),
				                                           alloc,
				                                           _block_buffer,
				                                           _block_buffer_count,
				                                           _block,
				                                           _info.block_size,
				                                           _info.block_count,
				                                           _tx_source,
				                                           _info.writeable);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result const result = Single_file_system::stat(path, out);
			out.size = _info.block_count * _info.block_size;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs::Block_file_system::Local_factory : File_system_factory
{
	typedef Genode::String<64> Label;
	Label const _label;

	Name const _name;

	Vfs::Env &_env;

	Genode::Allocator_avl _tx_block_alloc { &_env.alloc() };

	Block::Connection<> _block {
		_env.env(), &_tx_block_alloc, 128*1024, _label.string() };

	Block::Session::Info const _info { _block.info() };

	Genode::Io_signal_handler<Local_factory> _block_signal_handler {
		_env.env().ep(), *this, &Local_factory::_handle_block_signal };

	void _handle_block_signal() { }

	Data_file_system _data_fs;
	
	struct Info : Block::Session::Info
	{
		void print(Genode::Output &out) const
		{
			char buf[128] { };
			Genode::Xml_generator xml(buf, sizeof(buf), "block", [&] () {
				xml.attribute("count", Block::Session::Info::block_count);
				xml.attribute("size",  Block::Session::Info::block_size);
			});
			Genode::print(out, Genode::Cstring(buf));
		}
	};

	Readonly_value_file_system<Info>             _info_fs        { "info",        Info { } };
	Readonly_value_file_system<Genode::uint64_t> _block_count_fs { "block_count", 0 };
	Readonly_value_file_system<size_t>           _block_size_fs  { "block_size",  0 };

	static Name name(Xml_node config)
	{
		return config.attribute_value("name", Name("block"));
	}

	static unsigned buffer_count(Xml_node config)
	{
		return config.attribute_value("block_buffer_count", 1U);
	}

	Local_factory(Vfs::Env &env, Xml_node config)
	:
		_label   { config.attribute_value("label", Label("")) },
		_name    { name(config) },
		_env     { env },
		_data_fs { _env, _block, _info, name(config), buffer_count(config) }
	{
		_block.sigh(_block_signal_handler);
		_info_fs       .value(Info { _info });
		_block_count_fs.value(_info.block_count);
		_block_size_fs .value(_info.block_size);
	}

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type("data")) {
			return &_data_fs;
		}

		if (node.has_type("info")) {
			return &_info_fs;
		}

		if (node.has_type("block_count")) {
			return &_block_count_fs;
		}

		if (node.has_type("block_size")) {
			return &_block_size_fs;
		}

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
			Genode::Xml_generator xml(buf, sizeof(buf), "compound", [&] () {

				xml.node("data", [&] () {
					xml.attribute("name", name); });

				xml.node("dir", [&] () {
					xml.attribute("name", Name(".", name));
					xml.node("info",  [&] () {});
					xml.node("block_count", [&] () {});
					xml.node("block_size",  [&] () {});
				});
			});

			return Config(Genode::Cstring(buf));
		}

	public:

		Compound_file_system(Vfs::Env &vfs_env, Genode::Xml_node node)
		:
			Local_factory { vfs_env, node },
			Vfs::Dir_file_system { vfs_env,
			                       Xml_node(_config(Local_factory::name(node)).string()),
			                       *this }
		{ }

		static const char *name() { return "block"; }

		char const *type() override { return name(); }
};

#endif /* _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_ */

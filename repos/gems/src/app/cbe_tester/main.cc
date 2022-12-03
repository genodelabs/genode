/*
 * \brief  Tool for running tests and benchmarks on the CBE
 * \author Martin Stein
 * \date   2020-08-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <block_session/connection.h>
#include <vfs/simple_env.h>

/* CBE includes */
#include <cbe/library.h>
#include <cbe/check/library.h>
#include <cbe/dump/library.h>
#include <cbe/dump/configuration.h>
#include <cbe/init/library.h>
#include <cbe/init/configuration.h>

/* CBE tester includes */
#include <crypto.h>
#include <trust_anchor.h>
#include <verbose_node.h>

using namespace Genode;
using namespace Cbe;
using namespace Vfs;


enum class Module_type : uint8_t
{
	CBE_INIT,
	CBE_DUMP,
	CBE_CHECK,
	CBE,
	CMD_POOL,
};


static Module_type module_type_from_uint32(uint32_t uint32)
{
	class Bad_tag { };
	switch (uint32) {
	case 1: return Module_type::CBE_INIT;
	case 2: return Module_type::CBE;
	case 3: return Module_type::CBE_DUMP;
	case 4: return Module_type::CBE_CHECK;
	case 5: return Module_type::CMD_POOL;
	default: throw Bad_tag { };
	}
}


static uint32_t module_type_to_uint32(Module_type type)
{
	class Bad_type { };
	switch (type) {
	case Module_type::CBE_INIT : return 1;
	case Module_type::CBE      : return 2;
	case Module_type::CBE_DUMP : return 3;
	case Module_type::CBE_CHECK: return 4;
	case Module_type::CMD_POOL : return 5;
	}
	throw Bad_type { };
}


static Module_type tag_get_module_type(uint32_t tag)
{
	return module_type_from_uint32((tag >> 24) & 0xff);
}


static uint32_t tag_set_module_type(uint32_t    tag,
                                    Module_type type)
{
	if (tag >> 24) {

		class Bad_tag { };
		throw Bad_tag { };
	}
	return tag | (module_type_to_uint32(type) << 24);
}


static uint32_t tag_unset_module_type(uint32_t tag)
{
	return tag & 0xffffff;
}


static char const *blk_pkt_op_to_string(Block::Packet_descriptor::Opcode op)
{
	switch (op) {
	case Block::Packet_descriptor::READ: return "read";
	case Block::Packet_descriptor::WRITE: return "write";
	case Block::Packet_descriptor::SYNC: return "sync";
	case Block::Packet_descriptor::TRIM: return "trim";
	case Block::Packet_descriptor::END: return "end";
	};
	return "?";
}


static String<128> blk_pkt_to_string(Block::Packet_descriptor const &packet)
{
	return
		String<128>(
			"op=", blk_pkt_op_to_string(packet.operation()),
			" vba=", packet.block_number(),
			" cnt=", packet.block_count(),
			" succ=", packet.succeeded(),
			" tag=", Hex(packet.tag().value));
}


template <typename T>
T read_attribute(Xml_node const &node,
                 char     const *attr)
{
	T value { };

	if (!node.has_attribute(attr)) {

		error("<", node.type(), "> node misses attribute '", attr, "'");
		class Attribute_missing { };
		throw Attribute_missing { };
	}
	if (!node.attribute(attr).value(value)) {

		error("<", node.type(), "> node has malformed '", attr, "' attribute");
		class Malformed_attribute { };
		throw Malformed_attribute { };
	}
	return value;
}


static void print_blk_data(Block_data const &blk_data)
{
	for(unsigned long idx = 0; idx < Cbe::BLOCK_SIZE; idx += 64) {
		log(
			"  ", idx, ": ",
			Hex(blk_data.values[idx + 0], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 1], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 2], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 3], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 4], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 5], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 6], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 7], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 8], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 9], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 10], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 11], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 12], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 13], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 14], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 15], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 16], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 17], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 18], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 19], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 20], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 21], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 22], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 23], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 24], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 25], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 26], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 27], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 28], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 29], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 30], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 31], Hex::OMIT_PREFIX, Hex::PAD));
	}
}


class Block_io : public Interface
{
	public:

		virtual bool request_acceptable() = 0;

		virtual void submit_request(Cbe::Request const &cbe_req,
		                            Block_data         &data) = 0;

		virtual void execute(Constructible<Cbe::Library> &cbe,
		                     Cbe_init::Library           &cbe_init,
		                     Cbe_dump::Library           &cbe_dump,
		                     Cbe_check::Library          &cbe_check,
		                     Verbose_node          const &verbose_node,
		                     Io_buffer                   &blk_buf,
		                     bool                        &progress) = 0;
};


class Vfs_block_io_job
{
	private:

		using file_size = Vfs::file_size;
		using file_offset = Vfs::file_offset;

		enum State { PENDING, IN_PROGRESS, COMPLETE };

		Vfs::Vfs_handle &_handle;
		Cbe::Request     _cbe_req;
		State            _state;
		file_offset      _nr_of_processed_bytes;
		file_size        _nr_of_remaining_bytes;

		Cbe::Io_buffer::Index _cbe_req_io_buf_idx(Cbe::Request const &cbe_req)
		{
			return
				Cbe::Io_buffer::Index {
					(uint32_t)cbe_req.tag() & 0xffffff };
		}

		void
		_mark_req_completed_at_module(Constructible<Cbe::Library> &cbe,
		                              Cbe_init::Library           &cbe_init,
		                              Cbe_dump::Library           &cbe_dump,
		                              Cbe_check::Library          &cbe_check,
		                              Verbose_node          const &verbose_node,
		                              bool                        &progress)
		{
			Cbe::Io_buffer::Index const data_index {
				_cbe_req_io_buf_idx(_cbe_req) };

			switch (tag_get_module_type(_cbe_req.tag())) {
			case Module_type::CBE_INIT:

				cbe_init.io_request_completed(data_index, _cbe_req.success());
				break;

			case Module_type::CBE:

				cbe->io_request_completed(data_index, _cbe_req.success());
				break;

			case Module_type::CBE_DUMP:

				cbe_dump.io_request_completed(data_index, _cbe_req.success());
				break;

			case Module_type::CBE_CHECK:

				cbe_check.io_request_completed(data_index, _cbe_req.success());
				break;

			case Module_type::CMD_POOL:

				class Bad_module_type { };
				throw Bad_module_type { };
			}
			progress = true;

			if (verbose_node.blk_io_req_completed()) {
				log("blk req completed: ", _cbe_req);
			}
		}


		void _execute_read(Constructible<Cbe::Library> &cbe,
		                   Cbe_init::Library           &cbe_init,
		                   Cbe_dump::Library           &cbe_dump,
		                   Cbe_check::Library          &cbe_check,
		                   Verbose_node          const &verbose_node,
		                   Io_buffer                   &io_data,
		                   bool                        &progress)
		{
			using Result = Vfs::File_io_service::Read_result;

			switch (_state) {
			case State::PENDING:

				_handle.seek(_cbe_req.block_number() * Cbe::BLOCK_SIZE +
				             _nr_of_processed_bytes);

				if (!_handle.fs().queue_read(&_handle, _nr_of_remaining_bytes)) {
					return;
				}
				_state = State::IN_PROGRESS;
				progress = true;
				return;

			case State::IN_PROGRESS:
			{
				file_size nr_of_read_bytes { 0 };

				char *const data {
					reinterpret_cast<char *>(
						&io_data.item(_cbe_req_io_buf_idx(_cbe_req))) };

				Result const result {
					_handle.fs().complete_read(&_handle,
					                           data + _nr_of_processed_bytes,
					                           _nr_of_remaining_bytes,
					                           nr_of_read_bytes) };

				switch (result) {
				case Result::READ_QUEUED:
				case Result::READ_ERR_INTERRUPT:
				case Result::READ_ERR_AGAIN:
				case Result::READ_ERR_WOULD_BLOCK:

					return;

				case Result::READ_OK:

					_nr_of_processed_bytes += nr_of_read_bytes;
					_nr_of_remaining_bytes -= nr_of_read_bytes;

					if (_nr_of_remaining_bytes == 0) {

						_state = State::COMPLETE;
						_cbe_req.success(true);

						_mark_req_completed_at_module(
							cbe, cbe_init, cbe_dump, cbe_check,
							verbose_node, progress);

						progress = true;
						return;

					} else {

						_state = State::PENDING;
						progress = true;
						return;
					}

				case Result::READ_ERR_IO:
				case Result::READ_ERR_INVALID:

					_state = State::COMPLETE;
					_cbe_req.success(false);

					_mark_req_completed_at_module(
						cbe, cbe_init, cbe_dump, cbe_check,
						verbose_node, progress);

					progress = true;
					return;

				default:

					class Bad_complete_read_result { };
					throw Bad_complete_read_result { };
				}
			}
			default: return;
			}
		}

		void _execute_write(Constructible<Cbe::Library> &cbe,
		                    Cbe_init::Library           &cbe_init,
		                    Cbe_dump::Library           &cbe_dump,
		                    Cbe_check::Library          &cbe_check,
		                    Verbose_node          const &verbose_node,
		                    Io_buffer                   &io_data,
		                    bool                        &progress)
		{
			using Result = Vfs::File_io_service::Write_result;

			switch (_state) {
			case State::PENDING:

				_handle.seek(_cbe_req.block_number() * Cbe::BLOCK_SIZE +
				             _nr_of_processed_bytes);

				_state = State::IN_PROGRESS;
				progress = true;
				break;

			case State::IN_PROGRESS:
			{
				file_size nr_of_written_bytes { 0 };

				char const *const data {
					reinterpret_cast<char *>(
						&io_data.item(_cbe_req_io_buf_idx(_cbe_req))) };

				Result result;
				try {
					result = _handle.fs().write(&_handle,
					                            data + _nr_of_processed_bytes,
					                            _nr_of_remaining_bytes,
					                            nr_of_written_bytes);

				} catch (Vfs::File_io_service::Insufficient_buffer) {

					return;
				}
				switch (result) {
				case Result::WRITE_ERR_AGAIN:
				case Result::WRITE_ERR_INTERRUPT:
				case Result::WRITE_ERR_WOULD_BLOCK:

					return;

				case Result::WRITE_OK:

					_nr_of_processed_bytes += nr_of_written_bytes;
					_nr_of_remaining_bytes -= nr_of_written_bytes;

					if (_nr_of_remaining_bytes == 0) {

						_state = State::COMPLETE;
						_cbe_req.success(true);

						_mark_req_completed_at_module(
							cbe, cbe_init, cbe_dump, cbe_check,
							verbose_node, progress);

						progress = true;
						return;

					} else {

						_state = State::PENDING;
						progress = true;
						return;
					}

				case Result::WRITE_ERR_IO:
				case Result::WRITE_ERR_INVALID:

					_state = State::COMPLETE;
					_cbe_req.success(false);

					_mark_req_completed_at_module(
						cbe, cbe_init, cbe_dump, cbe_check,
						verbose_node, progress);

					progress = true;
					return;

				default:

					class Bad_write_result { };
					throw Bad_write_result { };
				}

			}
			default: return;
			}
		}

		void _execute_sync(Constructible<Cbe::Library> &cbe,
		                   Cbe_init::Library           &cbe_init,
		                   Cbe_dump::Library           &cbe_dump,
		                   Cbe_check::Library          &cbe_check,
		                   Verbose_node          const &verbose_node,
		                   bool                        &progress)
		{
			using Result = Vfs::File_io_service::Sync_result;

			switch (_state) {
			case State::PENDING:

				if (!_handle.fs().queue_sync(&_handle)) {
					return;
				}
				_state = State::IN_PROGRESS;
				progress = true;
				break;;

			case State::IN_PROGRESS:

				switch (_handle.fs().complete_sync(&_handle)) {
				case Result::SYNC_QUEUED:

					return;

				case Result::SYNC_ERR_INVALID:

					_cbe_req.success(false);
					_mark_req_completed_at_module(
						cbe, cbe_init, cbe_dump, cbe_check, verbose_node,
						progress);

					_state = State::COMPLETE;
					progress = true;
					return;

				case Result::SYNC_OK:

					_cbe_req.success(true);
					_mark_req_completed_at_module(
						cbe, cbe_init, cbe_dump, cbe_check, verbose_node,
						progress);

					_state = State::COMPLETE;
					progress = true;
					return;

				default:

					class Bad_sync_result { };
					throw Bad_sync_result { };
				}

			default: return;
			}
		}

	public:

		Vfs_block_io_job(Vfs::Vfs_handle &handle,
		                 Cbe::Request     cbe_req)
		:
			_handle                { handle },
			_cbe_req               { cbe_req },
			_state                 { State::PENDING },
			_nr_of_processed_bytes { 0 },
			_nr_of_remaining_bytes { _cbe_req.count() * Cbe::BLOCK_SIZE }
		{ }

		bool complete() const
		{
			return _state == COMPLETE;
		}

		void execute(Constructible<Cbe::Library> &cbe,
		             Cbe_init::Library           &cbe_init,
		             Cbe_dump::Library           &cbe_dump,
		             Cbe_check::Library          &cbe_check,
		             Verbose_node          const &verbose_node,
		             Io_buffer                   &blk_buf,
		             bool                        &progress)
		{
			using Cbe_operation = Cbe::Request::Operation;

			switch (_cbe_req.operation()) {
			case Cbe_operation::READ:

				_execute_read(cbe, cbe_init, cbe_dump, cbe_check, verbose_node,
				              blk_buf, progress);

				break;

			case Cbe_operation::WRITE:

				_execute_write(cbe, cbe_init, cbe_dump, cbe_check, verbose_node,
				               blk_buf, progress);

				break;

			case Cbe_operation::SYNC:

				_execute_sync(cbe, cbe_init, cbe_dump, cbe_check, verbose_node,
				              progress);

				break;

			default:

				class Bad_cbe_operation { };
				throw Bad_cbe_operation { };
			}
		}
};


class Vfs_block_io : public Block_io
{
	private:

		String<32>                const  _path;
		Vfs::Env                        &_vfs_env;
		Vfs_io_response_handler          _vfs_io_response_handler;
		Vfs::Vfs_handle                 &_vfs_handle { *_init_vfs_handle(_vfs_env, _path) };
		Constructible<Vfs_block_io_job>  _job        { };

		Vfs_block_io(const Vfs_block_io&);

		const Vfs_block_io& operator=(const Vfs_block_io&);

		static Vfs::Vfs_handle *_init_vfs_handle(Vfs::Env         &vfs_env,
		                                         String<32> const &path)
		{
			using Result = Vfs::Directory_service::Open_result;

			Vfs::Vfs_handle *vfs_handle { nullptr };
			Result const result {
				vfs_env.root_dir().open(
					path.string(), Vfs::Directory_service::OPEN_MODE_RDWR,
					&vfs_handle, vfs_env.alloc()) };

			if (result != Result::OPEN_OK) {

				class Open_failed { };
				throw Open_failed { };
			}
			return vfs_handle;
		}

	public:

		Vfs_block_io(Vfs::Env                  &vfs_env,
		             Xml_node            const &block_io,
		             Signal_context_capability  sigh)
		:
			_path                    { block_io.attribute_value(
			                              "path", String<32> { "" } ) },

			_vfs_env                 { vfs_env },
			_vfs_io_response_handler { sigh }
		{
			_vfs_handle.handler(&_vfs_io_response_handler);
		}


		/**************
		 ** Block_io **
		 **************/

		bool request_acceptable() override
		{
			return !_job.constructed();
		}

		void submit_request(Cbe::Request const &cbe_req,
		                    Block_data         &) override
		{
			_job.construct(_vfs_handle, cbe_req);
		}

		void execute(Constructible<Cbe::Library> &cbe,
		             Cbe_init::Library           &cbe_init,
		             Cbe_dump::Library           &cbe_dump,
		             Cbe_check::Library          &cbe_check,
		             Verbose_node          const &verbose_node,
		             Io_buffer                   &blk_buf,
		             bool                        &progress) override
		{
			if (!_job.constructed()) {
				return;
			}
			_job->execute(
				cbe, cbe_init, cbe_dump, cbe_check, verbose_node, blk_buf,
				progress);

			if (_job->complete()) {
				_job.destruct();
			}
		}
};


class Block_connection_block_io : public Block_io
{
	private:

		enum { TX_BUF_SIZE = Block::Session::TX_QUEUE_SIZE * BLOCK_SIZE, };

		Genode::Env         &_env;
		Heap                &_heap;
		Allocator_avl        _blk_alloc { &_heap };
		Block::Connection<>  _blk       { _env, &_blk_alloc, TX_BUF_SIZE };

		Cbe::Io_buffer::Index _packet_io_buf_idx(Block::Packet_descriptor const & pkt)
		{
			return
				Cbe::Io_buffer::Index {
					(uint32_t)pkt.tag().value & 0xffffff };
		}

		static Block::Packet_descriptor::Opcode
		_cbe_op_to_block_op(Cbe::Request::Operation cbe_op)
		{
			switch (cbe_op) {
			case Cbe::Request::Operation::READ:  return Block::Packet_descriptor::READ;
			case Cbe::Request::Operation::WRITE: return Block::Packet_descriptor::WRITE;
			case Cbe::Request::Operation::SYNC:  return Block::Packet_descriptor::SYNC;
			default:
				class Bad_cbe_op { };
				throw Bad_cbe_op { };
			}
		}

	public:

		Block_connection_block_io(Genode::Env               &env,
		                          Heap                      &heap,
		                          Signal_context_capability  sigh)
		:
			_env  { env },
			_heap { heap }
		{
			_blk.tx_channel()->sigh_ack_avail(sigh);
			_blk.tx_channel()->sigh_ready_to_submit(sigh);
		}

		~Block_connection_block_io()
		{
			_blk.tx_channel()->sigh_ack_avail(Signal_context_capability());
			_blk.tx_channel()->sigh_ready_to_submit(Signal_context_capability());
		}


		/**************
		 ** Block_io **
		 **************/

		bool request_acceptable() override
		{
			return _blk.tx()->ready_to_submit();
		}

		void submit_request(Cbe::Request const &cbe_req,
		                    Block_data         &data) override
		{
			Block::Packet_descriptor::Opcode const blk_op {
				_cbe_op_to_block_op(cbe_req.operation()) };

			Block::Packet_descriptor const packet {
				_blk.alloc_packet(Cbe::BLOCK_SIZE),
				blk_op,
				cbe_req.block_number(),
				cbe_req.count(),
				Block::Packet_descriptor::Tag { cbe_req.tag() } };
			if (cbe_req.operation() == Cbe::Request::Operation::WRITE) {

				*reinterpret_cast<Cbe::Block_data*>(
					_blk.tx()->packet_content(packet)) = data;
			}
			_blk.tx()->try_submit_packet(packet);
		}

		void execute(Constructible<Cbe::Library> &cbe,
		             Cbe_init::Library           &cbe_init,
		             Cbe_dump::Library           &cbe_dump,
		             Cbe_check::Library          &cbe_check,
		             Verbose_node          const &verbose_node,
		             Io_buffer                   &blk_buf,
		             bool                        &progress) override
		{
			while (_blk.tx()->ack_avail()) {

				Block::Packet_descriptor packet {
					_blk.tx()->try_get_acked_packet() };

				Cbe::Io_buffer::Index const data_index {
					_packet_io_buf_idx(packet) };

				if (packet.operation() == Block::Packet_descriptor::READ &&
					packet.succeeded())
				{
					blk_buf.item(data_index) =
						*reinterpret_cast<Cbe::Block_data*>(
							_blk.tx()->packet_content(packet));
				}
				switch (tag_get_module_type(packet.tag().value)) {
				case Module_type::CBE_INIT:

					cbe_init.io_request_completed(data_index,
					                               packet.succeeded());
					break;

				case Module_type::CBE:

					cbe->io_request_completed(data_index,
					                          packet.succeeded());
					break;

				case Module_type::CBE_DUMP:

					cbe_dump.io_request_completed(data_index,
					                               packet.succeeded());
					break;

				case Module_type::CBE_CHECK:

					cbe_check.io_request_completed(data_index,
					                               packet.succeeded());
					break;

				case Module_type::CMD_POOL:

					class Bad_module_type { };
					throw Bad_module_type { };
				}
				_blk.tx()->release_packet(packet);
				progress = true;

				if (verbose_node.blk_io_req_completed()) {
					log("blk pkt completed: ", blk_pkt_to_string(packet));
				}
			}
			_blk.tx()->wakeup();
		}
};


class Log_node
{
	private:

		String<128> const _string;

	public:

		Log_node(Xml_node const &node)
		:
			_string { node.attribute_value("string", String<128> { }) }
		{ }

		String<128> const &string() const { return _string; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "string=\"", _string, "\"");
		}
};


class Benchmark_node
{
	public:

		using Label = String<128>;

		enum Operation { START, STOP };

	private:

		Operation const _op;
		bool      const _label_avail;
		Label     const _label;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("start")) {
				return Operation::START;
			}
			if (node.attribute("op").has_value("stop")) {
				return Operation::STOP;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

		static char const *_op_to_string(Operation op)
		{
			switch (op) {
			case START: return "start";
			case STOP: return "stop";
			}
			return "?";
		}

	public:

		bool has_attr_label() const
		{
			return _op == Operation::START;
		}

		Benchmark_node(Xml_node const &node)
		:
			_op          { _read_op_attr(node) },
			_label_avail { has_attr_label() && node.has_attribute("label") },
			_label       { _label_avail ?
			               node.attribute_value("label", Label { }) :
			               Label { } }
		{ }

		Operation op() const { return _op; }
		bool label_avail() const { return _label_avail; }
		Label const &label() const { return _label; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=", _op_to_string(_op));
			if (_label_avail) {
				Genode::print(out, " label=", _label);
			}
		}
};


class Benchmark
{
	private:

		enum State { STARTED, STOPPED };

		Genode::Env                   &_env;
		Timer::Connection              _timer                   { _env };
		State                          _state                   { STOPPED };
		Microseconds                   _start_time              { 0 };
		uint64_t                       _nr_of_virt_blks_read    { 0 };
		uint64_t                       _nr_of_virt_blks_written { 0 };
		Constructible<Benchmark_node>  _start_node              { };
		uint64_t                       _id                      { 0 };

	public:

		Benchmark(Genode::Env &env) : _env { env } { }

		void submit_request(Benchmark_node const &node)
		{
			switch (node.op()) {
			case Benchmark_node::START:

				if (_state != STOPPED) {
					class Bad_state_to_start { };
					throw Bad_state_to_start { };
				}
				_id++;
				_nr_of_virt_blks_read = 0;
				_nr_of_virt_blks_written = 0;
				_state = STARTED;
				_start_node.construct(node);
				_start_time = _timer.curr_time().trunc_to_plain_us();
				break;

			case Benchmark_node::STOP:

				if (_state != STARTED) {
					class Bad_state_to_stop { };
					throw Bad_state_to_stop { };
				}
				uint64_t const stop_time_us {
					_timer.curr_time().trunc_to_plain_us().value };

				log("");
				if (_start_node->label_avail()) {
					log("Benchmark result \"", _start_node->label(), "\"");
				} else {
					log("Benchmark result (command ID ", _id, ")");
				}

				double const passed_time_sec {
					(double)(stop_time_us - _start_time.value) /
					(double)(1000 * 1000) };

				log("   Ran ", passed_time_sec, " seconds.");

				if (_nr_of_virt_blks_read != 0) {

					uint64_t const bytes_read {
						_nr_of_virt_blks_read * Cbe::BLOCK_SIZE };

					double const mibyte_read {
						(double)bytes_read / (double)(1024 * 1024) };

					double const mibyte_per_sec_read {
						(double)bytes_read / (double)passed_time_sec /
						(double)(1024 * 1024) };

					log("   Have read ", mibyte_read, " mebibyte in total.");
					log("   Have read ", mibyte_per_sec_read, " mebibyte per second.");
				}

				if (_nr_of_virt_blks_written != 0) {

					uint64_t bytes_written {
						_nr_of_virt_blks_written * Cbe::BLOCK_SIZE };

					double const mibyte_written {
						(double)bytes_written / (double)(1024 * 1024) };

					double const mibyte_per_sec_written {
						(double)bytes_written / (double)passed_time_sec /
						(double)(1024 * 1024) };

					log("   Have written ", mibyte_written, " mebibyte in total.");
					log("   Have written ", mibyte_per_sec_written, " mebibyte per second.");
				}
				log("");
				_state = STOPPED;
				break;
			}
		}

		void raise_nr_of_virt_blks_read()    { _nr_of_virt_blks_read++;    }
		void raise_nr_of_virt_blks_written() { _nr_of_virt_blks_written++; }
};


class Trust_anchor_node
{
	private:

		using Operation = Trust_anchor_request::Operation;

		Operation  const _op;
		String<64> const _passphrase;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("initialize")) {
				return Operation::INITIALIZE;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

	public:

		Trust_anchor_node(Xml_node const &node)
		:
			_op               { _read_op_attr(node) },
			_passphrase       { has_attr_passphrase() ?
			                    node.attribute_value("passphrase", String<64>()) :
			                    String<64>() }
		{ }

		Operation         op()         const { return _op; }
		String<64> const &passphrase() const { return _passphrase; }

		bool has_attr_passphrase() const
		{
			return _op == Operation::INITIALIZE;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=", to_string(_op));
			if (has_attr_passphrase()) {
				Genode::print(out, " passphrase=", _passphrase);
			}
		}
};


class Request_node
{
	private:

		using Operation = Cbe::Request::Operation;

		Operation             const _op;
		Virtual_block_address const _vba;
		Number_of_blocks      const _count;
		bool                  const _sync;
		bool                  const _salt_avail;
		uint64_t              const _salt;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("read")) {
				return Operation::READ;
			}
			if (node.attribute("op").has_value("write")) {
				return Operation::WRITE;
			}
			if (node.attribute("op").has_value("sync")) {
				return Operation::SYNC;
			}
			if (node.attribute("op").has_value("create_snapshot")) {
				return Operation::CREATE_SNAPSHOT;
			}
			if (node.attribute("op").has_value("extend_ft")) {
				return Operation::EXTEND_FT;
			}
			if (node.attribute("op").has_value("extend_vbd")) {
				return Operation::EXTEND_VBD;
			}
			if (node.attribute("op").has_value("rekey")) {
				return Operation::REKEY;
			}
			if (node.attribute("op").has_value("deinitialize")) {
				return Operation::DEINITIALIZE;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

	public:

		Request_node(Xml_node const &node)
		:
			_op         { _read_op_attr(node) },
			_vba        { has_attr_vba() ?
			              read_attribute<uint64_t>(node, "vba") : 0 },
			_count      { has_attr_count() ?
			              read_attribute<uint64_t>(node, "count") : 0 },
			_sync       { read_attribute<bool>(node, "sync") },
			_salt_avail { has_attr_salt() ?
			              node.has_attribute("salt") : false },
			_salt       { has_attr_salt() && _salt_avail ?
			              read_attribute<uint64_t>(node, "salt") : 0 }
		{ }

		Operation               op()         const { return _op; }
		Virtual_block_address   vba()        const { return _vba; }
		Number_of_blocks        count()      const { return _count; }
		bool                    sync()       const { return _sync; }
		bool                    salt_avail() const { return _salt_avail; }
		uint64_t                salt()       const { return _salt; }

		bool has_attr_vba() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE ||
			       _op == Operation::SYNC;
		}

		bool has_attr_salt() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE;
		}

		bool has_attr_count() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE ||
			       _op == Operation::SYNC ||
			       _op == Operation::EXTEND_FT ||
			       _op == Operation::EXTEND_VBD;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=", to_string(_op));
			if (has_attr_vba()) {
				Genode::print(out, " vba=", _vba);
			}
			if (has_attr_count()) {
				Genode::print(out, " count=", _count);
			}
			Genode::print(out, " sync=", _sync);
			if (_salt_avail) {
				Genode::print(out, " salt=", _salt);
			}
		}
};


class Command : public Fifo<Command>::Element
{
	public:

		enum Type
		{
			INVALID,
			REQUEST,
			TRUST_ANCHOR,
			BENCHMARK,
			CONSTRUCT,
			DESTRUCT,
			INITIALIZE,
			CHECK,
			DUMP,
			LIST_SNAPSHOTS,
			LOG
		};

		enum State
		{
			PENDING,
			IN_PROGRESS,
			COMPLETED
		};

	private:

		Type                                   _type              { INVALID };
		uint32_t                               _id                { 0 };
		State                                  _state             { PENDING };
		bool                                   _success           { false };
		bool                                   _data_mismatch     { false };
		Constructible<Request_node>            _request_node      { };
		Constructible<Trust_anchor_node>       _trust_anchor_node { };
		Constructible<Benchmark_node>          _benchmark_node    { };
		Constructible<Log_node>                _log_node          { };
		Constructible<Cbe_init::Configuration> _initialize        { };
		Constructible<Cbe_dump::Configuration> _dump              { };

		char const *_state_to_string() const
		{
			switch (_state) {
			case PENDING: return "pending";
			case IN_PROGRESS: return "in_progress";
			case COMPLETED: return "completed";
			}
			return "?";
		}

		char const *_type_to_string() const
		{
			switch (_type) {
			case INITIALIZE: return "initialize";
			case INVALID: return "invalid";
			case DUMP: return "dump";
			case REQUEST: return "request";
			case TRUST_ANCHOR: return "trust_anchor";
			case BENCHMARK: return "benchmark";
			case CONSTRUCT: return "construct";
			case DESTRUCT: return "destruct";
			case CHECK: return "check";
			case LIST_SNAPSHOTS: return "list_snapshots";
			case LOG: return "log";
			}
			return "?";
		}

	public:

		Command() { }

		Command(Type            type,
		        Xml_node const &node,
		        uint32_t        id)
		:
			_type { type },
			_id   { id }
		{
			switch (_type) {
			case INITIALIZE:   _initialize.construct(node);        break;
			case DUMP:         _dump.construct(node);              break;
			case REQUEST:      _request_node.construct(node);      break;
			case TRUST_ANCHOR: _trust_anchor_node.construct(node); break;
			case BENCHMARK:    _benchmark_node.construct(node);    break;
			case LOG:          _log_node.construct(node);          break;
			default:                                               break;
			}
		}

		Command(Command &other)
		:
			_type    { other._type },
			_id      { other._id },
			_state   { other._state },
			_success { other._success }
		{
			switch (_type) {
			case INITIALIZE:   _initialize.construct(*other._initialize);               break;
			case DUMP:         _dump.construct(*other._dump);                           break;
			case REQUEST:      _request_node.construct(*other._request_node);           break;
			case TRUST_ANCHOR: _trust_anchor_node.construct(*other._trust_anchor_node); break;
			case BENCHMARK:    _benchmark_node.construct(*other._benchmark_node);       break;
			case LOG:          _log_node.construct(*other._log_node);                   break;
			default:                                                                    break;
			}
		}

		bool has_attr_data_mismatch() const
		{
			return
				_type == REQUEST &&
				_request_node->op() == Cbe::Request::Operation::READ &&
				_request_node->salt_avail();
		}

		bool synchronize() const
		{
			class Bad_type { };
			switch (_type) {
			case INITIALIZE:     return true;
			case BENCHMARK:      return true;
			case CONSTRUCT:      return true;
			case DESTRUCT:       return true;
			case DUMP:           return true;
			case CHECK:          return true;
			case TRUST_ANCHOR:   return true;
			case LIST_SNAPSHOTS: return true;
			case LOG:            return true;
			case REQUEST:        return _request_node->sync();
			case INVALID:        throw Bad_type { };
			}
			throw Bad_type { };
		}

		static Type type_from_string(String<64> str)
		{
			if (str == "initialize")     { return INITIALIZE; }
			if (str == "request")        { return REQUEST; }
			if (str == "trust-anchor")   { return TRUST_ANCHOR; }
			if (str == "benchmark")      { return BENCHMARK; }
			if (str == "construct")      { return CONSTRUCT; }
			if (str == "destruct")       { return DESTRUCT; }
			if (str == "check")          { return CHECK; }
			if (str == "dump")           { return DUMP; }
			if (str == "list-snapshots") { return LIST_SNAPSHOTS; }
			if (str == "log")            { return LOG; }
			class Bad_string { };
			throw Bad_string { };
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "id=", _id, " type=", _type_to_string());
			class Bad_type { };
			switch (_type) {
			case INITIALIZE:     Genode::print(out, " cfg=(", *_initialize, ")"); break;
			case REQUEST:        Genode::print(out, " cfg=(", *_request_node, ")"); break;
			case TRUST_ANCHOR:   Genode::print(out, " cfg=(", *_trust_anchor_node, ")"); break;
			case BENCHMARK:      Genode::print(out, " cfg=(", *_benchmark_node, ")"); break;
			case DUMP:           Genode::print(out, " cfg=(", *_dump, ")"); break;
			case LOG:            Genode::print(out, " cfg=(", *_log_node, ")"); break;
			case INVALID:        break;
			case CHECK:          break;
			case CONSTRUCT:      break;
			case DESTRUCT:       break;
			case LIST_SNAPSHOTS: break;
			}
			Genode::print(out, " succ=", _success);
			if (has_attr_data_mismatch()) {
				Genode::print(out, " bad_data=", _data_mismatch);
			}
			Genode::print(out, " state=", _state_to_string());
		}

		Type                           type              () const { return _type              ; }
		State                          state             () const { return _state             ; }
		uint32_t                       id                () const { return _id                ; }
		bool                           success           () const { return _success           ; }
		bool                           data_mismatch     () const { return _data_mismatch     ; }
		Request_node            const &request_node      () const { return *_request_node     ; }
		Trust_anchor_node       const &trust_anchor_node () const { return *_trust_anchor_node; }
		Benchmark_node          const &benchmark_node    () const { return *_benchmark_node   ; }
		Log_node                const &log_node          () const { return *_log_node         ; }
		Cbe_init::Configuration const &initialize        () const { return *_initialize       ; }
		Cbe_dump::Configuration const &dump              () const { return *_dump             ; }

		void state         (State state)        { _state = state; }
		void success       (bool success)       { _success = success; }
		void data_mismatch (bool data_mismatch) { _data_mismatch = data_mismatch; }

};


class Command_pool {

	private:

		Allocator          &_alloc;
		Verbose_node const &_verbose_node;
		Fifo<Command>       _cmd_queue              { };
		uint32_t            _next_command_id        { 0 };
		unsigned long       _nr_of_uncompleted_cmds { 0 };
		unsigned long       _nr_of_errors           { 0 };
		Block_data          _blk_data               { };

		void _read_cmd_node(Xml_node const &node,
		                    Command::Type   cmd_type)
		{
			Command &cmd {
				*new (_alloc) Command(cmd_type, node, _next_command_id++) };

			_nr_of_uncompleted_cmds++;
			_cmd_queue.enqueue(cmd);

			if (_verbose_node.cmd_pool_cmd_pending()) {
				log("cmd pending: ", cmd);
			}
		}

		static void _generate_blk_data(Block_data            &blk_data,
		                               Virtual_block_address  vba,
		                               uint64_t               salt)
		{
			for (uint64_t idx { 0 };
			     idx + sizeof(vba) + sizeof(salt) <=
			        sizeof(blk_data.values) / sizeof(blk_data.values[0]); )
			{
				memcpy(&blk_data.values[idx], &vba, sizeof(vba));
				idx += sizeof(vba);
				memcpy(&blk_data.values[idx], &salt, sizeof(salt));
				idx += sizeof(salt);
				vba += idx + salt;
				salt += idx + vba;
			}
		}

	public:

		Command_pool(Allocator          &alloc,
		             Xml_node     const &config_xml,
		             Verbose_node const &verbose_node)
		:
			_alloc        { alloc },
			_verbose_node { verbose_node }
		{
			config_xml.sub_node("commands").for_each_sub_node(
				[&] (Xml_node const &node)
			{
				_read_cmd_node(node, Command::type_from_string(node.type()));
			});
		}

		Command peek_pending_command(Command::Type type) const
		{
			Reconstructible<Command> resulting_cmd { };
			bool first_uncompleted_cmd { true };
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &curr_cmd)
			{
				if (exit_loop) {
					return;
				}
				switch (curr_cmd.state()) {
				case Command::PENDING:

					/*
					 * Stop iterating at the first uncompleted command
					 * that needs to be synchronized.
					 */
					if (curr_cmd.synchronize()) {
						if (curr_cmd.type() == type && first_uncompleted_cmd) {
							resulting_cmd.construct(curr_cmd);
						}
						exit_loop = true;
						return;
					}
					/*
					 * Select command and stop iterating if the command is of
					 * the desired type.
					 */
					if (curr_cmd.type() == type) {
						resulting_cmd.construct(curr_cmd);
						exit_loop = true;
					}
					first_uncompleted_cmd = false;
					return;

				case Command::IN_PROGRESS:

					/*
					 * Stop iterating at the first uncompleted command
					 * that needs to be synchronized.
					 */
					if (curr_cmd.synchronize()) {
						exit_loop = true;
						return;
					}
					first_uncompleted_cmd = false;
					return;

				case Command::COMPLETED:

					return;
				}
			});
			return *resulting_cmd;
		}

		void mark_command_in_progress(unsigned long cmd_id)
		{
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (exit_loop) {
					return;
				}
				if (cmd.id() == cmd_id) {
					if (cmd.state() != Command::PENDING) {
						class Bad_state { };
						throw Bad_state { };
					}
					cmd.state(Command::IN_PROGRESS);
					exit_loop = true;

					if (_verbose_node.cmd_pool_cmd_in_progress()) {
						log("cmd in progress: ", cmd);
					}
				}
			});
		}

		void mark_command_completed(unsigned long cmd_id,
		                            bool          success)
		{
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (exit_loop) {
					return;
				}
				if (cmd.id() == cmd_id) {

					if (cmd.state() != Command::IN_PROGRESS) {

						class Bad_state { };
						throw Bad_state { };
					}
					cmd.state(Command::COMPLETED);
					_nr_of_uncompleted_cmds--;
					cmd.success(success);
					if (!cmd.success()) {
						_nr_of_errors++;
					}
					exit_loop = true;

					if (_verbose_node.cmd_pool_cmd_completed()) {
						log("cmd completed: ", cmd);
					}
				}
			});
		}

		void generate_blk_data(Cbe::Request           cbe_req,
		                       Virtual_block_address  vba,
		                       Block_data            &blk_data) const
		{
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (exit_loop) {
					return;
				}
				if (cmd.id() != cbe_req.tag()) {
					return;
				}
				if (cmd.type() != Command::REQUEST) {
					class Bad_command_type { };
					throw Bad_command_type { };
				}
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail()) {

					_generate_blk_data(blk_data, vba, req_node.salt());
				}
				exit_loop = true;
			});
		}

		void verify_blk_data(Cbe::Request           cbe_req,
		                     Virtual_block_address  vba,
		                     Block_data      const &blk_data)
		{
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (exit_loop) {
					return;
				}
				if (cmd.id() != cbe_req.tag()) {
					return;
				}
				if (cmd.type() != Command::REQUEST) {
					class Bad_command_type { };
					throw Bad_command_type { };
				}
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail()) {

					Block_data gen_blk_data { };
					_generate_blk_data(gen_blk_data, vba, req_node.salt());

					if (memcmp(blk_data.values, gen_blk_data.values,
					           sizeof(blk_data.values) /
					           sizeof(blk_data.values[0]))) {

						cmd.data_mismatch(true);
						_nr_of_errors++;

						if (_verbose_node.client_data_mismatch()) {
							log("client data mismatch: vba=", vba,
							    " req=(", cbe_req, ")");
							log("client data should be:");
							print_blk_data(gen_blk_data);
							log("client data is:");
							print_blk_data(blk_data);
							class Client_data_mismatch { };
							throw Client_data_mismatch { };
						}
					}
				}
				exit_loop = true;
			});
		}

		void print_failed_cmds() const
		{
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (cmd.state() != Command::COMPLETED) {
					return;
				}
				if (cmd.success() &&
				    (!cmd.has_attr_data_mismatch() || !cmd.data_mismatch())) {

					return;
				}
				log("cmd failed: ", cmd);
			});
		}

		unsigned long nr_of_uncompleted_cmds() { return _nr_of_uncompleted_cmds; }
		unsigned long nr_of_errors()           { return _nr_of_errors; }
};


class Main
{
	private:

		Genode::Env                 &_env;
		Attached_rom_dataspace       _config_rom           { _env, "config" };
		Verbose_node                 _verbose_node         { _config_rom.xml() };
		Heap                         _heap                 { _env.ram(), _env.rm() };
		Vfs::Simple_env              _vfs_env              { _env, _heap, _config_rom.xml().sub_node("vfs") };
		Signal_handler<Main>         _sigh                 { _env.ep(), *this, &Main::_execute };
		Block_io                    &_blk_io               { _init_blk_io(_config_rom.xml(), _heap, _env, _vfs_env, _sigh) };
		Io_buffer                    _blk_buf              { };
		Command_pool                 _cmd_pool             { _heap, _config_rom.xml(), _verbose_node };
		Constructible<Cbe::Library>  _cbe                  { };
		Cbe_check::Library           _cbe_check            { };
		Cbe_dump::Library            _cbe_dump             { };
		Cbe_init::Library            _cbe_init             { };
		Benchmark                    _benchmark            { _env };
		Trust_anchor                 _trust_anchor         { _vfs_env, _config_rom.xml().sub_node("trust-anchor"), _sigh };
		Crypto_plain_buffer          _crypto_plain_buf     { };
		Crypto_cipher_buffer         _crypto_cipher_buf    { };
		Crypto                       _crypto               { _vfs_env,
		                                                     _config_rom.xml().sub_node("crypto"),
		                                                     _sigh };

		Block_io &_init_blk_io(Xml_node            const &config,
		                       Heap                      &heap,
		                       Genode::Env               &env,
		                       Vfs::Env                  &vfs_env,
		                       Signal_context_capability  sigh)
		{
			Xml_node const &block_io { config.sub_node("block-io") };
			if (block_io.attribute("type").has_value("block_connection")) {
				return *new (heap)
					Block_connection_block_io { env, heap, sigh };
			}
			if (block_io.attribute("type").has_value("vfs")) {
				return *new (heap)
					Vfs_block_io {
						vfs_env, block_io, sigh };
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

		template <typename MODULE>
		void _handle_pending_blk_io_requests_of_module(MODULE      &module,
		                                               Module_type  module_type,
		                                               bool        &progress)
		{
			while (true) {

				if (!_blk_io.request_acceptable()) {
					break;
				}
				Cbe::Io_buffer::Index data_index { 0 };
				Cbe::Request cbe_req { };
				module.has_io_request(cbe_req, data_index);
				if (!cbe_req.valid()) {
					break;
				}
				if (data_index.value & 0xff000000) {
					class Bad_data_index { };
					throw Bad_data_index { };
				}
				cbe_req.tag(
					tag_set_module_type(data_index.value, module_type));

				_blk_io.submit_request(cbe_req, _blk_buf.item(data_index));

				if (_verbose_node.blk_io_req_in_progress()) {
					log("blk req in progress: ", cbe_req);
				}
				module.io_request_in_progress(data_index);
				progress = true;
			}
		}

		template <typename MODULE>
		void _handle_completed_client_requests_of_module(MODULE &module,
		                                                 bool   &progress)
		{
			while (true) {

				Cbe::Request const cbe_req {
					module.peek_completed_client_request() };

				if (!cbe_req.valid()) {
					break;
				}
				_cmd_pool.mark_command_completed(cbe_req.tag(),
				                                 cbe_req.success());

				module.drop_completed_client_request(cbe_req);
				progress = true;
			}
		}

		void _execute_cbe_dump (bool &progress)
		{
			_cbe_dump.execute(_blk_buf);
			if (_cbe_dump.execute_progress()) {
				progress = true;
			}
			_handle_pending_blk_io_requests_of_module(
				_cbe_dump, Module_type::CBE_DUMP, progress);

			_handle_completed_client_requests_of_module(_cbe_dump, progress);
		}

		template <typename MODULE>
		void _handle_pending_ta_requests_of_module(MODULE      &module,
		                                           Module_type  module_type,
		                                           bool        &progress)
		{
			using Ta_operation = Cbe::Trust_anchor_request::Operation;
			while (true) {

				if (!_trust_anchor.request_acceptable()) {
					break;
				}
				Cbe::Trust_anchor_request ta_req =
					module.peek_generated_ta_request();

				if (ta_req.operation() == Ta_operation::INVALID) {
					return;
				}
				Cbe::Trust_anchor_request typed_ta_req { ta_req };
				typed_ta_req.tag(
					tag_set_module_type(typed_ta_req.tag(), module_type));

				if (_verbose_node.ta_req_in_progress()) {
					log("ta req in progress: ", typed_ta_req);
				}
				switch (ta_req.operation()) {
				case Ta_operation::CREATE_KEY:

					_trust_anchor.submit_request(typed_ta_req);
					module.drop_generated_ta_request(ta_req);
					progress = true;
					break;

				case Ta_operation::SECURE_SUPERBLOCK:

					_trust_anchor.submit_request_hash(
						typed_ta_req,
						module.peek_generated_ta_sb_hash(ta_req));

					module.drop_generated_ta_request(ta_req);
					progress = true;
					break;

				case Ta_operation::ENCRYPT_KEY:

					_trust_anchor.submit_request_key_plaintext_value(
						typed_ta_req,
						module.peek_generated_ta_key_value_plaintext(ta_req));

					module.drop_generated_ta_request(ta_req);
					progress = true;
					break;

				case Ta_operation::DECRYPT_KEY:

					_trust_anchor.submit_request_key_ciphertext_value(
						typed_ta_req,
						module.peek_generated_ta_key_value_ciphertext(ta_req));

					module.drop_generated_ta_request(ta_req);
					progress = true;
					break;

				case Ta_operation::LAST_SB_HASH:

					_trust_anchor.submit_request(typed_ta_req);
					module.drop_generated_ta_request(ta_req);
					progress = true;
					break;

				default:

					class Bad_operation { };
					throw Bad_operation { };
				}
			}
		}

		void _execute_cbe_init(bool &progress)
		{
			_cbe_init.execute(_blk_buf);
			if (_cbe_init.execute_progress()) {
				progress = true;
			}
			_handle_pending_blk_io_requests_of_module(
				_cbe_init, Module_type::CBE_INIT, progress);

			_handle_pending_ta_requests_of_module(
				_cbe_init, Module_type::CBE_INIT, progress);

			_handle_completed_client_requests_of_module(_cbe_init, progress);
		}

		void _cbe_transfer_client_data_that_was_read(bool &progress)
		{
			while (true) {

				Cbe::Request request { };
				uint64_t vba { 0 };
				Crypto_plain_buffer::Index plain_buf_idx { 0 };
				_cbe->client_transfer_read_data_required(
					request, vba, plain_buf_idx);

				if (!request.valid()) {
					break;
				}
				_cmd_pool.verify_blk_data(
					request, vba, _crypto_plain_buf.item(plain_buf_idx));

				_cbe->client_transfer_read_data_in_progress(plain_buf_idx);
				_cbe->client_transfer_read_data_completed(plain_buf_idx, true);
				_benchmark.raise_nr_of_virt_blks_read();
				progress = true;

				if (_verbose_node.client_data_transferred()) {
					log("client data: vba=", vba, " req=(", request, ")");
				}
			}
		}

		void _cbe_transfer_client_data_that_will_be_written(bool &progress)
		{
			while (true) {

				Cbe::Request request { };
				uint64_t vba { 0 };
				Crypto_plain_buffer::Index plain_buf_idx { 0 };
				_cbe->client_transfer_write_data_required(
					request, vba, plain_buf_idx);

				if (!request.valid()) {
					return;
				}
				_cmd_pool.generate_blk_data(
					request, vba, _crypto_plain_buf.item(plain_buf_idx));

				_cbe->client_transfer_write_data_in_progress(plain_buf_idx);
				_cbe->client_transfer_write_data_completed(
					plain_buf_idx, true);

				_benchmark.raise_nr_of_virt_blks_written();
				progress = true;

				if (_verbose_node.client_data_transferred()) {
					log("client data: vba=", vba, " req=(", request, ")");
				}
			}
		}

		void _cbe_handle_crypto_add_key_requests(bool &progress)
		{
			while (true) {

				Key key;
				Cbe::Request request { _cbe->crypto_add_key_required(key) };
				if (!request.valid()) {
					return;
				}
				switch (_crypto.add_key(key)) {
				case Crypto::Result::SUCCEEDED:

					if (_verbose_node.crypto_req_in_progress()) {
						log("crypto req in progress: ", request);
					}
					_cbe->crypto_add_key_requested(request);

					if (_verbose_node.crypto_req_completed()) {
						log("crypto req completed: ", request);
					}
					request.success(true);
					_cbe->crypto_add_key_completed(request);
					progress = true;
					break;

				case Crypto::Result::FAILED:

					class Add_key_failed { };
					throw Add_key_failed { };

				case Crypto::Result::RETRY_LATER:

					return;
				}
			}
		}

		void _cbe_handle_crypto_remove_key_requests(bool &progress)
		{
			while (true) {

				Key::Id key_id;
				Cbe::Request request {
					_cbe->crypto_remove_key_required(key_id) };

				if (!request.valid()) {
					break;
				}
				switch (_crypto.remove_key(key_id)) {
				case Crypto::Result::SUCCEEDED:

					if (_verbose_node.crypto_req_in_progress()) {
						log("crypto req in progress: ", request);
					}
					_cbe->crypto_remove_key_requested(request);

					if (_verbose_node.crypto_req_completed()) {
						log("crypto req completed: ", request);
					}
					request.success(true);
					_cbe->crypto_remove_key_completed(request);
					progress = true;
					break;

				case Crypto::Result::FAILED:

					class Remove_key_failed { };
					throw Remove_key_failed { };

				case Crypto::Result::RETRY_LATER:

					return;
				}
			}
		}

		void _cbe_handle_crypto_encrypt_requests(bool &progress)
		{
			while (true) {

				if (!_crypto.request_acceptable()) {
					break;
				}
				Crypto_plain_buffer::Index data_index { 0 };
				Cbe::Request request {
					_cbe->crypto_cipher_data_required(data_index) };

				if (!request.valid()) {
					break;
				}
				request.tag(data_index.value);
				_crypto.submit_request(
				    request, Crypto::Operation::ENCRYPT_BLOCK,
				    data_index,
				    Crypto_cipher_buffer::Index { data_index.value });

				_cbe->crypto_cipher_data_requested(data_index);
				if (_verbose_node.crypto_req_in_progress()) {
					log("crypto req in progress: ", request);
				}
				progress = true;
			}
		}

		void _cbe_handle_crypto_decrypt_requests(bool &progress)
		{
			while (true) {

				if (!_crypto.request_acceptable()) {
					break;
				}
				Crypto_cipher_buffer::Index data_index { 0 };
				Cbe::Request request {
					_cbe->crypto_plain_data_required(data_index) };

				if (!request.valid()) {
					break;
				}
				request.tag(data_index.value);
				_crypto.submit_request(
				    request, Crypto::Operation::DECRYPT_BLOCK,
				    Crypto_plain_buffer::Index { data_index.value },
				    data_index);

				_cbe->crypto_plain_data_requested(data_index);
				if (_verbose_node.crypto_req_in_progress()) {
					log("crypto req in progress: ", request);
				}
				progress = true;
			}
		}

		void _cbe_handle_crypto_requests(bool &progress)
		{
			_cbe_handle_crypto_add_key_requests(progress);
			_cbe_handle_crypto_remove_key_requests(progress);
			_cbe_handle_crypto_encrypt_requests(progress);
			_cbe_handle_crypto_decrypt_requests(progress);
		}

		void _execute_cbe(bool &progress)
		{
			_cbe->execute(_blk_buf, _crypto_plain_buf, _crypto_cipher_buf);
			if (_cbe->execute_progress()) {
				progress = true;
			}
			_handle_pending_blk_io_requests_of_module(
				*_cbe, Module_type::CBE, progress);

			_handle_pending_ta_requests_of_module(
				*_cbe, Module_type::CBE, progress);

			_cbe_handle_crypto_requests(progress);
			_cbe_transfer_client_data_that_was_read(progress);
			_cbe_transfer_client_data_that_will_be_written(progress);
			_handle_completed_client_requests_of_module(*_cbe, progress);
		}

		void _cmd_pool_handle_pending_cbe_init_cmds(bool &progress)
		{
			while (true) {

				if (!_cbe_init.client_request_acceptable()) {
					break;
				}
				Command const cmd {
					_cmd_pool.peek_pending_command(Command::INITIALIZE) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				Cbe_init::Configuration const &cfg { cmd.initialize() };

				_cbe_init.submit_client_request(
					Cbe::Request(
						Cbe::Request::Operation::READ,
						false, 0, 0, 0, 0, cmd.id()),
					cfg.vbd_nr_of_lvls() - 1,
					cfg.vbd_nr_of_children(),
					cfg.vbd_nr_of_leafs(),
					cfg.ft_nr_of_lvls() - 1,
					cfg.ft_nr_of_children(),
					cfg.ft_nr_of_leafs());

				_cmd_pool.mark_command_in_progress(cmd.id());
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_check_cmds(bool &progress)
		{
			while (true) {

				if (!_cbe_check.client_request_acceptable()) {
					break;
				}
				Command const cmd {
					_cmd_pool.peek_pending_command(Command::CHECK) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_cbe_check.submit_client_request(
					Cbe::Request {
						Cbe::Request::Operation::READ,
						false,
						0,
						0,
						0,
						0,
						cmd.id()
					}
				);
				_cmd_pool.mark_command_in_progress(cmd.id());
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_cbe_cmds(bool &progress)
		{
			while (true) {

				if (!_cbe->client_request_acceptable()) {
					break;
				}
				Command const cmd {
					_cmd_pool.peek_pending_command(Command::REQUEST) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				Request_node const &req_node { cmd.request_node() };
				Cbe::Request const &cbe_req {
					cmd.request_node().op(),
					false,
					req_node.has_attr_vba() ? req_node.vba() : 0,
					0,
					req_node.has_attr_count() ? req_node.count() : 0,
					0,
					cmd.id() };

				_cbe->submit_client_request(cbe_req, 0);
				_cmd_pool.mark_command_in_progress(cmd.id());
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_ta_cmds(bool &progress)
		{
			while (true) {

				if (!_trust_anchor.request_acceptable()) {
					break;
				}
				Command const cmd {
					_cmd_pool.peek_pending_command(Command::TRUST_ANCHOR) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				Trust_anchor_node const &node { cmd.trust_anchor_node() };
				Trust_anchor_request const &ta_req {
					node.op(), false, cmd.id() };

				Trust_anchor_request typed_ta_req { ta_req };
				typed_ta_req.tag(
					tag_set_module_type(
						typed_ta_req.tag(), Module_type::CMD_POOL));

				switch (node.op()) {
				case Trust_anchor_request::Operation::INITIALIZE:

					_trust_anchor.submit_request_passphrase(
						typed_ta_req, node.passphrase());

					_cmd_pool.mark_command_in_progress(cmd.id());
					progress = true;

					break;

				default:

					class Bad_operation { };
					throw Bad_operation { };
				}
			}
		}

		void _cmd_pool_handle_pending_dump_cmds(bool &progress)
		{
			while (true) {

				if (!_cbe_dump.client_request_acceptable()) {
					break;
				}
				Command const cmd {
					_cmd_pool.peek_pending_command(Command::DUMP) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				Cbe_dump::Configuration const &cfg { cmd.dump() };
				_cbe_dump.submit_client_request(
					Cbe::Request(
						Cbe::Request::Operation::READ,
						false, 0, 0, 0, 0, cmd.id()),
					cfg);

				_cmd_pool.mark_command_in_progress(cmd.id());
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_construct_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::CONSTRUCT) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_cbe.construct();
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_destruct_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::DESTRUCT) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_cbe.destruct();
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_list_snapshots_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::LIST_SNAPSHOTS) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				Active_snapshot_ids ids;
				_cbe->active_snapshot_ids(ids);
				unsigned snap_nr { 0 };
				log("");
				log("List snapshots (command ID ", cmd.id(), ")");
				for (unsigned idx { 0 }; idx < sizeof(ids.values) / sizeof(ids.values[0]); idx++) {
					if (ids.values[idx] != 0) {
						log("   Snapshot #", snap_nr, " is generation ",
						    (uint64_t)ids.values[idx]);

						snap_nr++;
					}
				}
				log("");
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_log_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::LOG) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				log("\n", cmd.log_node().string(), "\n");
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_benchmark_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::BENCHMARK) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_benchmark.submit_request(cmd.benchmark_node());
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _execute_cbe_check (bool &progress)
		{
			_cbe_check.execute(_blk_buf);
			if (_cbe_check.execute_progress()) {
				progress = true;
			}
			_handle_pending_blk_io_requests_of_module(
				_cbe_check, Module_type::CBE_CHECK, progress);

			_handle_completed_client_requests_of_module(_cbe_check, progress);
		}

		void _execute_command_pool(bool &progress)
		{
			if (_cbe.constructed()) {
				_cmd_pool_handle_pending_cbe_cmds(progress);
				_cmd_pool_handle_pending_list_snapshots_cmds(progress);
			}
			_cmd_pool_handle_pending_log_cmds(progress);
			_cmd_pool_handle_pending_ta_cmds(progress);
			_cmd_pool_handle_pending_cbe_init_cmds(progress);
			_cmd_pool_handle_pending_benchmark_cmds(progress);
			_cmd_pool_handle_pending_construct_cmds(progress);
			_cmd_pool_handle_pending_destruct_cmds(progress);
			_cmd_pool_handle_pending_dump_cmds(progress);
			_cmd_pool_handle_pending_check_cmds(progress);

			if (_cmd_pool.nr_of_uncompleted_cmds() == 0) {

				if (_cmd_pool.nr_of_errors() > 0) {

					_cmd_pool.print_failed_cmds();
					_env.parent().exit(-1);

				} else {

					_env.parent().exit(0);
				}
			}
		}

		template <typename MODULE>
		void
		_trust_anchor_handle_completed_requests_of_module(MODULE                     &module,
		                                                  Trust_anchor_request const &typed_ta_req,
		                                                  bool                       &progress)
		{
			using Ta_operation = Cbe::Trust_anchor_request::Operation;

			Trust_anchor_request ta_req { typed_ta_req };
			ta_req.tag(tag_unset_module_type(ta_req.tag()));

			if (_verbose_node.ta_req_completed()) {
				log("ta req completed: ", typed_ta_req);
			}
			switch (ta_req.operation()) {
			case Ta_operation::CREATE_KEY:

				module.mark_generated_ta_create_key_request_complete(
					ta_req,
					_trust_anchor.peek_completed_key_plaintext_value());

				_trust_anchor.drop_completed_request();
				progress = true;
				break;

			case Ta_operation::SECURE_SUPERBLOCK:

				module.mark_generated_ta_secure_sb_request_complete(
					ta_req);

				_trust_anchor.drop_completed_request();
				progress = true;
				break;

			case Ta_operation::LAST_SB_HASH:

				module.mark_generated_ta_last_sb_hash_request_complete(
					ta_req,
					_trust_anchor.peek_completed_hash());

				_trust_anchor.drop_completed_request();
				progress = true;
				break;

			case Ta_operation::ENCRYPT_KEY:

				module.mark_generated_ta_encrypt_key_request_complete(
					ta_req,
					_trust_anchor.peek_completed_key_ciphertext_value());

				_trust_anchor.drop_completed_request();
				progress = true;
				break;

			case Ta_operation::DECRYPT_KEY:

				module.mark_generated_ta_decrypt_key_request_complete(
					ta_req,
					_trust_anchor.peek_completed_key_plaintext_value());

				_trust_anchor.drop_completed_request();
				progress = true;
				break;

			default:

				class Bad_ta_operation { };
				throw Bad_ta_operation { };
			}
		}

		void _trust_anchor_handle_completed_requests(bool &progress)
		{
			while (true) {

				Trust_anchor_request const typed_ta_req {
					_trust_anchor.peek_completed_request() };

				if (!typed_ta_req.valid()) {
					break;
				}
				switch (tag_get_module_type(typed_ta_req.tag())) {
				case Module_type::CMD_POOL:
				{
					Trust_anchor_request ta_req { typed_ta_req };
					ta_req.tag(tag_unset_module_type(ta_req.tag()));

					using Ta_operation = Trust_anchor_request::Operation;
					if (ta_req.operation() == Ta_operation::INITIALIZE) {

						_cmd_pool.mark_command_completed(ta_req.tag(),
						                                 ta_req.success());

						_trust_anchor.drop_completed_request();
						progress = true;
						continue;

					} else {

						class Bad_operation { };
						throw Bad_operation { };
					}
					break;
				}
				case Module_type::CBE_INIT:

					_trust_anchor_handle_completed_requests_of_module(
						_cbe_init, typed_ta_req, progress);

					break;

				case Module_type::CBE:

					_trust_anchor_handle_completed_requests_of_module(
						*_cbe, typed_ta_req, progress);

					break;

				default:

					class Bad_module_type { };
					throw Bad_module_type { };
				}
			}
		}

		void _execute_trust_anchor(bool &progress)
		{
			_trust_anchor.execute(progress);
			_trust_anchor_handle_completed_requests(progress);
		}

		void _crypto_handle_completed_encrypt_requests(bool &progress)
		{
			while (true) {

				Cbe::Request const request {
					_crypto.peek_completed_encryption_request() };

				if (!request.valid()) {
					break;
				}
				Crypto_cipher_buffer::Index const data_idx { request.tag() };
				_cbe->supply_crypto_cipher_data(data_idx, request.success());

				_crypto.drop_completed_request();
				progress = true;

				if (_verbose_node.crypto_req_completed()) {
					log("crypto req completed: ", request);
				}
			}
		}

		void _crypto_handle_completed_decrypt_requests(bool &progress)
		{
			while (true) {

				Cbe::Request const request {
					_crypto.peek_completed_decryption_request() };

				if (!request.valid()) {
					break;
				}
				Crypto_plain_buffer::Index const data_idx { request.tag() };
				_cbe->supply_crypto_plain_data(data_idx, request.success());

				_crypto.drop_completed_request();
				progress = true;

				if (_verbose_node.crypto_req_completed()) {
					log("crypto req completed: ", request);
				}
			}
		}

		void _execute_crypto(bool &progress)
		{
			_crypto.execute(_crypto_plain_buf, _crypto_cipher_buf, progress);
			_crypto_handle_completed_encrypt_requests(progress);
			_crypto_handle_completed_decrypt_requests(progress);
		}

		void _execute()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				_execute_command_pool(progress);
				_execute_cbe_init(progress);

				_blk_io.execute(
					_cbe, _cbe_init, _cbe_dump, _cbe_check, _verbose_node,
					_blk_buf, progress);

				_execute_trust_anchor(progress);
				_execute_cbe_check(progress);
				_execute_cbe_dump(progress);
				_execute_crypto(progress);
				if (_cbe.constructed()) {
					_execute_cbe(progress);
				}
			}
			_vfs_env.io().commit();
		}

	public:

		Main(Genode::Env &env)
		:
			_env { env }
		{
			_execute();
		}
};

void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();

	Cbe::assert_valid_object_size<Cbe::Library>();
	cbe_cxx_init();

	Cbe::assert_valid_object_size<Cbe_init::Library>();
	cbe_init_cxx_init();

	Cbe::assert_valid_object_size<Cbe_check::Library>();
	cbe_check_cxx_init();

	Cbe::assert_valid_object_size<Cbe_dump::Library>();
	cbe_dump_cxx_init();

	static Main main(env);
}

extern "C" int memcmp(const void *p0, const void *p1, Genode::size_t size)
{
	return Genode::memcmp(p0, p1, size);
}

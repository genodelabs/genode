/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
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
#include <block_session/connection.h>
#include <timer_session/connection.h>

/* CBE-dump includes */
#include <cbe/dump/library.h>

using namespace Genode;


class Main
{
	private:

		enum { TX_BUF_SIZE = Block::Session::TX_QUEUE_SIZE * Cbe::BLOCK_SIZE };

		Env                  &_env;
		Heap                  _heap        { _env.ram(), _env.rm() };
		Allocator_avl         _blk_alloc   { &_heap };
		Block::Connection<>   _blk         { _env, &_blk_alloc, TX_BUF_SIZE };
		Signal_handler<Main>  _blk_handler { _env.ep(), *this, &Main::_execute };
		Cbe::Request          _blk_req     { };
		Cbe::Io_buffer        _blk_buf     { };
		Cbe_dump::Library    _cbe_dump    { };

		Genode::size_t        _blk_ratio   {
			Cbe::BLOCK_SIZE / _blk.info().block_size };

		void _execute()
		{
			for (bool progress { true }; progress; ) {

				progress = false;

				_cbe_dump.execute(_blk_buf);
				if (_cbe_dump.execute_progress()) {
					progress = true;
				}

				Cbe::Request const req {
					_cbe_dump.peek_completed_client_request() };

				if (req.valid()) {
					_cbe_dump.drop_completed_client_request(req);
					if (req.success()) {
						_env.parent().exit(0);
					} else {
						error("request was not successful");;
						_env.parent().exit(-1);
					}
				}

				struct Invalid_io_request : Exception { };

				while (_blk.tx()->ready_to_submit()) {

					Cbe::Io_buffer::Index data_index { 0 };
					Cbe::Request request { };
					_cbe_dump.has_io_request(request, data_index);

					if (!request.valid()) {
						break;
					}
					if (_blk_req.valid()) {
						break;
					}
					try {
						request.tag(data_index.value);
						Block::Packet_descriptor::Opcode op;
						switch (request.operation()) {
						case Cbe::Request::Operation::READ:
							op = Block::Packet_descriptor::READ;
							break;
						case Cbe::Request::Operation::WRITE:
							op = Block::Packet_descriptor::WRITE;
							break;
						default:
							throw Invalid_io_request();
						}
						Block::Packet_descriptor packet {
							_blk.alloc_packet(Cbe::BLOCK_SIZE), op,
							request.block_number() * _blk_ratio,
							request.count() * _blk_ratio };

						if (request.operation() == Cbe::Request::Operation::WRITE) {
							*reinterpret_cast<Cbe::Block_data*>(
								_blk.tx()->packet_content(packet)) =
									_blk_buf.item(data_index);
						}
						_blk.tx()->try_submit_packet(packet);
						_blk_req = request;
						_cbe_dump.io_request_in_progress(data_index);
						progress = true;
					}
					catch (Block::Session::Tx::Source::Packet_alloc_failed) {
						break;
					}
				}

				while (_blk.tx()->ack_avail()) {

					Block::Packet_descriptor packet =
						_blk.tx()->try_get_acked_packet();

					if (!_blk_req.valid()) {
						break;
					}

					bool const read  =
						packet.operation() == Block::Packet_descriptor::READ;

					bool const write =
						packet.operation() == Block::Packet_descriptor::WRITE;

					bool const op_match =
						(read && _blk_req.read()) ||
						(write && _blk_req.write());

					bool const bn_match =
						packet.block_number() / _blk_ratio == _blk_req.block_number();

					if (!bn_match || !op_match) {
						break;
					}

					_blk_req.success(packet.succeeded());

					Cbe::Io_buffer::Index const data_index { _blk_req.tag() };
					bool                  const success    { _blk_req.success() };

					if (read && success) {
						_blk_buf.item(data_index) =
							*reinterpret_cast<Cbe::Block_data*>(
								_blk.tx()->packet_content(packet));
					}
					_cbe_dump.io_request_completed(data_index, success);
					_blk.tx()->release_packet(packet);
					_blk_req = Cbe::Request { };
					progress = true;
				}
			}
			_blk.tx()->wakeup();
		}

	public:

		Main(Env &env)
		:
			_env { env }
		{
			if (_blk_ratio == 0) {
				error("backend block size not supported");
				_env.parent().exit(-1);
				return;
			}

			if (!_cbe_dump.client_request_acceptable()) {
				error("failed to submit request");
				_env.parent().exit(-1);
			}

			Attached_rom_dataspace config_rom { _env, "config" };

			try {
				Xml_node const &config {
					config_rom.xml().sub_node("dump") };

				Cbe_dump::Configuration const cfg { config };

				_cbe_dump.submit_client_request(
					Cbe::Request { Cbe::Request::Operation::READ,
					               false, 0, 0, 0, 0, 0 },
					cfg);
			}
			catch (Xml_node::Nonexistent_sub_node) {
				error("missing 'dump' config node");
				_env.parent().exit(1);
				return;
			}

			_blk.tx_channel()->sigh_ack_avail(_blk_handler);
			_blk.tx_channel()->sigh_ready_to_submit(_blk_handler);

			_execute();
		}

		~Main()
		{
			_blk.tx_channel()->sigh_ack_avail(Signal_context_capability());
			_blk.tx_channel()->sigh_ready_to_submit(Signal_context_capability());
		}
};


extern "C" int memcmp(const void *p0, const void *p1, Genode::size_t size)
{
	return Genode::memcmp(p0, p1, size);
}


extern "C" void adainit();


void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();
	Timer::Connection timer { env };

	Cbe::assert_valid_object_size<Cbe_dump::Library>();

	cbe_dump_cxx_init();

	static Main main(env);
}

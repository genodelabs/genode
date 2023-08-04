/*
 * \brief  Verify the dimensions and hashes of a tresor container
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

/* gems includes */
#include <tresor/check/library.h>

using namespace Genode;

namespace Tresor {

	char const *module_name(unsigned long)
	{
		return "?";
	}
}

class Main
{
	private:

		enum { TX_BUF_SIZE = Block::Session::TX_QUEUE_SIZE * Tresor::BLOCK_SIZE };

		Env                  &_env;
		Heap                  _heap        { _env.ram(), _env.rm() };
		Allocator_avl         _blk_alloc   { &_heap };
		Block::Connection<>   _blk         { _env, &_blk_alloc, TX_BUF_SIZE };
		Signal_handler<Main>  _blk_handler { _env.ep(), *this, &Main::_execute };
		Tresor::Request          _blk_req     { };
		Tresor::Io_buffer        _blk_buf     { };
		Tresor_check::Library    _tresor_check    { };

		Genode::size_t        _blk_ratio   {
			Tresor::BLOCK_SIZE / _blk.info().block_size };

		void _execute()
		{
			for (bool progress { true }; progress; ) {

				progress = false;

				_tresor_check.execute(_blk_buf);
				if (_tresor_check.execute_progress()) {
					progress = true;
				}

				Tresor::Request const req {
					_tresor_check.peek_completed_client_request() };

				if (req.valid()) {
					_tresor_check.drop_completed_client_request(req);
					if (req.success()) {
						_env.parent().exit(0);
					} else {
						error("request was not successful");;
						_env.parent().exit(-1);
					}
				}

				struct Invalid_io_request : Exception { };

				while (_blk.tx()->ready_to_submit()) {

					Tresor::Io_buffer::Index data_index { 0 };
					Tresor::Request request { };
					_tresor_check.has_io_request(request, data_index);

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
						case Tresor::Request::Operation::READ:
							op = Block::Packet_descriptor::READ;
							break;
						case Tresor::Request::Operation::WRITE:
							op = Block::Packet_descriptor::WRITE;
							break;
						default:
							throw Invalid_io_request();
						}
						Block::Packet_descriptor packet {
							_blk.alloc_packet(Tresor::BLOCK_SIZE), op,
							request.block_number() * _blk_ratio,
							request.count() * _blk_ratio };

						if (request.operation() == Tresor::Request::Operation::WRITE) {
							*reinterpret_cast<Tresor::Block*>(
								_blk.tx()->packet_content(packet)) =
									_blk_buf.item(data_index);
						}
						_blk.tx()->try_submit_packet(packet);
						_blk_req = request;
						_tresor_check.io_request_in_progress(data_index);
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

					Tresor::Io_buffer::Index const data_index { _blk_req.tag() };
					bool                  const success    { _blk_req.success() };

					if (read && success) {
						_blk_buf.item(data_index) =
							*reinterpret_cast<Tresor::Block*>(
								_blk.tx()->packet_content(packet));
					}
					_tresor_check.io_request_completed(data_index, success);
					_blk.tx()->release_packet(packet);
					_blk_req = Tresor::Request();
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

			if (!_tresor_check.client_request_acceptable()) {
				error("failed to submit request");
				_env.parent().exit(-1);
			}
			_tresor_check.submit_client_request(
				Tresor::Request(
					Tresor::Request::Operation::READ,
					false, 0, 0, 0, 0, 0));

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
	timer.msleep(3000);
	Genode::log("start checking");

	Tresor::assert_valid_object_size<Tresor_check::Library>();

	tresor_check_cxx_init();

	static Main main(env);
}

/*
 * \brief  Stream of block-operation requests
 * \author Norman Feske
 * \date   2018-12-17
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK__REQUEST_STREAM_H_
#define _INCLUDE__BLOCK__REQUEST_STREAM_H_

/* Genode includes */
#include <block_session/block_session.h>
#include <packet_stream_tx/rpc_object.h>
#include <block/request.h>

namespace Block { struct Request_stream; }


class Block::Request_stream : Genode::Noncopyable
{
	public:

		struct Block_size { Genode::size_t value; };
		struct Align_log2 { Genode::size_t value; };

		/**
		 * Interface for accessing the content of a 'Request'
		 *
		 * The 'Payload' is separate from 'Request_stream' to allow its use
		 * as argument without exposing the entirely of the 'Request_stream'
		 * to the called code.
		 */
		class Payload : Noncopyable
		{
			private:

				friend class Request_stream;

				Genode::addr_t       const _base;
				Genode::size_t       const _size;
				Block::Session::Info const _info;

				/**
				 * Return pointer to the first byte of the request content
				 */
				void *_request_ptr(Block::Request request) const
				{
					return (void *)(_base + request.offset);
				}

				/**
				 * Return request size in bytes
				 */
				Genode::size_t _request_size(Block::Request const &request) const
				{
					return request.operation.count * _info.block_size;
				}

				bool _valid_range_and_alignment(Block::Request const &request) const
				{
					/* local address of the last byte of the request */
					Genode::addr_t const request_end = _base + request.offset
					                                 + _request_size(request) - 1;

					/* check for integer overflow and zero-sized request */
					if (request_end <= _base)
						return false;

					/* check for upper bound */
					if (request_end > _base + _size - 1)
						return false;

					/* check for proper alignment */
					if (!Genode::aligned(request.offset, _info.align_log2))
						return false;

					return true;
				}

				Payload(Genode::addr_t base, Genode::size_t size,
				        Block::Session::Info info)
				:
					_base(base), _size(size), _info(info)
				{ }

			public:

				/**
				 * Call functor 'fn' with the pointer and size of the 'request'
				 * content as arguments
				 *
				 * If the request does not carry any payload, 'fn' is not
				 * called.
				 */
				template <typename FN>
				void with_content(Block::Request request, FN const &fn) const
				{
					if (_valid_range_and_alignment(request))
						fn(_request_ptr(request), _request_size(request));
				}
		};

	private:

		Block::Session::Info const _info;

		Packet_stream_tx::Rpc_object<Block::Session::Tx> _tx;

		typedef Genode::Packet_stream_sink<Block::Session::Tx_policy> Tx_sink;

		Payload const _payload;

	public:

		Request_stream(Genode::Region_map               &rm,
		               Genode::Dataspace_capability      ds,
		               Genode::Entrypoint               &ep,
		               Genode::Signal_context_capability sigh,
		               Block::Session::Info        const info)
		:
			_info(info),
			_tx(ds, rm, ep.rpc_ep()),
			_payload(_tx.sink()->ds_local_base(), _tx.sink()->ds_size(), info)
		{
			_tx.sigh_ready_to_ack(sigh);
			_tx.sigh_packet_avail(sigh);
		}

		virtual ~Request_stream()
		{
			_tx.sigh_ready_to_ack(Genode::Signal_context_capability());
			_tx.sigh_packet_avail(Genode::Signal_context_capability());
		}

		Genode::Capability<Block::Session::Tx> tx_cap() { return _tx.cap(); }

		Block::Session::Info info() const { return _info; }

		/**
		 * Call functor 'fn' with 'Payload' interface as argument
		 *
		 * The 'Payload' interface allows the functor to access the content
		 * of a request by calling 'Payload::with_content'.
		 */
		template <typename FN>
		void with_payload(FN const &fn) const { fn(_payload); }

		/**
		 * Call functor 'fn' with the pointer and size to the 'request' content
		 *
		 * This is a wrapper for 'Payload::with_content'. It is convenient
		 * in situations where the 'Payload' interface does not need to be
		 * propagated as argument.
		 */
		template <typename FN>
		void with_content(Request const &request, FN const &fn) const
		{
			_payload.with_content(request, fn);
		}

		enum class Response { ACCEPTED, REJECTED, RETRY };

		/**
		 * Call functor 'fn' for each pending request, with its packet as argument
		 *
		 * The boolean return value of the functor expresses whether the request
		 * was accepted or not. If it was accepted, the request is removed from the
		 * packet stream. If the request could not be accepted, the iteration
		 * aborts and the request packet stays in the packet stream.
		 */
		template <typename FN>
		void with_requests(FN const &fn)
		{
			Tx_sink &tx_sink = *_tx.sink();

			using namespace Genode;

			for (;;) {

				if (!tx_sink.packet_avail())
					return;

				typedef Block::Packet_descriptor Packet_descriptor;

				Packet_descriptor const packet = tx_sink.peek_packet();

				bool const packet_valid = tx_sink.packet_valid(packet)
				                       && (packet.offset() >= 0);

				Operation operation { .type         = packet.operation_type(),
				                      .block_number = packet.block_number(),
				                      .count        = packet.block_count() };

				Request request { .operation = operation,
				                  .success   = false,
				                  .offset    = packet.offset(),
				                  .tag       = packet.tag() };

				Response const response = packet_valid
				                        ? fn(request)
				                        : Response::REJECTED;
				bool progress = false;

				switch (response) {

				case Response::REJECTED:

					/*
					 * Acknowledge rejected packet if there is enough room in
					 * the acknowledgement queue. Otherwise, the rejected
					 * packet stays in the request queue and is evaluated
					 * again.
					 */
					if (tx_sink.ack_slots_free()) {
						(void)tx_sink.try_get_packet();
						tx_sink.try_ack_packet(packet);
						progress = true;
					}
					break;

				case Response::ACCEPTED:
					(void)tx_sink.try_get_packet();
					progress = true;
					break;

				case Response::RETRY:
					break;
				}

				/*
				 * Stop iterating of no request-queue elements can be consumed.
				 */
				if (!progress)
					break;
			}
		}

		class Ack : Noncopyable
		{
			private:

				friend class Request_stream;

				Tx_sink &_tx_sink;

				bool _submitted = false;

				Genode::size_t const _block_size;

				Ack(Tx_sink &tx_sink, Genode::size_t block_size)
				: _tx_sink(tx_sink), _block_size(block_size) { }

			public:

				void submit(Block::Request request)
				{
					if (_submitted) {
						Genode::warning("attempt to ack the same packet twice");
						return;
					}

					typedef Block::Packet_descriptor Packet_descriptor;
					Packet_descriptor::Payload
						payload { .offset = request.offset,
						          .bytes  = request.operation.count * _block_size };

					Packet_descriptor packet(request.operation, payload, request.tag);

					packet.succeeded(request.success);

					_tx_sink.try_ack_packet(packet);
					_submitted = true;
				}
		};

		/**
		 * Try to submit acknowledgement packets
		 *
		 * The method repeatedly calls the functor 'fn' with an 'Ack' reference,
		 * which provides an interface to 'submit' one acknowledgement. The
		 * iteration stops when the acknowledgement queue is fully populated or if
		 * the functor does not call 'Ack::submit'.
		 */
		template <typename FN>
		void try_acknowledge(FN const &fn)
		{
			Tx_sink &tx_sink = *_tx.sink();

			while (tx_sink.ack_slots_free()) {

				Ack ack(tx_sink, _payload._info.block_size);

				fn(ack);

				if (!ack._submitted)
					break;
			}
		}

		void wakeup_client_if_needed() { _tx.sink()->wakeup(); }
};

#endif /* _INCLUDE__BLOCK__REQUEST_STREAM_H_ */

/*
 * \brief  Connection to block service
 * \author Norman Feske
 * \date   2019-04-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK_SESSION__CONNECTION_H_
#define _INCLUDE__BLOCK_SESSION__CONNECTION_H_

#include <block_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>
#include <base/id_space.h>
#include <util/fifo.h>

namespace Block { template <typename> struct Connection; }


template <typename JOB = void>
struct Block::Connection : Genode::Connection<Session>, Session_client
{
	public:

		class Job;

		typedef Genode::size_t size_t;

	private:

		/*
		 * Define internally used '_JOB' type that corresponds to the 'JOB'
		 * template argument but falls back to 'Job' if no template argument
		 * is given.
		 */
		template <typename T, typename> struct Fallback           { typedef T  Type; };
		template <typename FB>          struct Fallback<void, FB> { typedef FB Type; };

		typedef typename Fallback<JOB, Job>::Type _JOB;

		typedef Genode::Id_space<_JOB> Tag_id_space;

		typedef Packet_descriptor::Payload Payload;

	public:

		class Job : Genode::Noncopyable
		{
			private:

				friend class Connection;

				Connection &_connection;

				Operation const _operation;

				/*
				 * Block offset relative to '_operation.block_number', used
				 * when performing large read/write operations in multiple
				 * steps.
				 */
				block_count_t _position = 0;

				/*
				 * Packet-stream allocation used for read/write operations
				 */
				Payload _payload { };

				bool _completed = false;

				/*
				 * A job undergoes three stages. The transition from one
				 * stage to the next happens 'update_jobs'.
				 *
				 * Initially, it resides in the '_pending' fifo queue.
				 *
				 * Once submitted, it is removed from the '_pending' fifo
				 * and added to the '_tags' ID space.
				 *
				 * Once acknowledged and complete, it is removed from the
				 * '_tags' ID space.
				 */

				Genode::Constructible<typename Tag_id_space::Element> _tag { };

				Genode::Fifo_element<_JOB> _pending_elem {
					*static_cast<_JOB *>(this) };

				Operation _curr_operation() const
				{
					if (!Operation::has_payload(_operation.type))
						return _operation;

					return {
						.type         = _operation.type,
						.block_number = _operation.block_number + _position,
						.count        = Genode::min(_connection._max_block_count,
						                            _operation.count - _position) };
				}

				template <typename FN>
				static void _with_offset_and_length(Job &job, FN const &fn)
				{
					if (!Operation::has_payload(job._operation.type))
						return;

					Operation const operation  = job._curr_operation();
					size_t    const block_size = job._connection._info.block_size;

					fn(operation.block_number * block_size,
					   Genode::min(job._payload.bytes, operation.count * block_size));
				}

				template <typename POLICY>
				void _submit(POLICY &policy, _JOB &job, Tx::Source &tx)
				{
					if (!_tag.constructed())
						return;

					Request::Tag const tag { _tag->id().value };

					Packet_descriptor const p(_curr_operation(), _payload, tag);

					if (_operation.type == Operation::Type::WRITE)
						_with_offset_and_length(job, [&] (off_t offset, size_t length) {
							policy.produce_write_content(job, offset,
							                             tx.packet_content(p),
							                             length); });
					tx.try_submit_packet(p);
				}

			public:

				Job(Connection &connection, Operation operation)
				:
					_connection(connection), _operation(operation)
				{
					_connection._pending.enqueue(_pending_elem);
				}

				~Job()
				{
					if (pending()) {
						_connection._pending.remove(_pending_elem);
					}
					else if (in_progress()) {
						Genode::warning("block-session job prematurely destructed");
					}
				}

				bool in_progress() const { return _tag.constructed(); }
				bool completed()   const { return _completed; }
				bool pending()     const { return !in_progress() && !completed(); }

				Operation operation() const { return _operation; }
		};

	private:

		block_count_t const _max_block_count;

		block_count_t _init_max_block_count(size_t buf_size) const
		{
			/*
			 * Number of bytes that may be unusable at the beginning or
			 * and of the buffer because of alignment constraints.
			 */
			size_t const align_scrap = 2*(1UL << _info.align_log2);

			if (buf_size < align_scrap)
				return 0;

			buf_size -= align_scrap;

			return buf_size / _info.block_size;
		}

		/**
		 * Tag ID space of submitted jobs
		 */
		Tag_id_space _tags { };

		/**
		 * Jobs that are pending for submission
		 */
		Genode::Fifo<Genode::Fifo_element<_JOB> > _pending { };

		/**
		 * Process a single acknowledgement
		 *
		 * \return true if progress was made
		 */
		template <typename POLICY>
		bool _try_process_ack(POLICY &, Tx::Source &);

		/**
		 * Submit next pending job to server, if possible
		 *
		 * \return true if a job was successfully submitted
		 */
		template <typename POLICY>
		bool _try_submit_pending_job(POLICY &, Tx::Source &);

	public:

		/**
		 * Constructor
		 *
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 * \param tx_buf_size      size of transmission buffer in bytes
		 */
		Connection(Genode::Env             &env,
		           Genode::Range_allocator *tx_block_alloc,
		           Genode::size_t           tx_buf_size = 128*1024,
		           const char              *label = "")
		:
			Genode::Connection<Session>(env,
				session(env.parent(),
				        "ram_quota=%ld, cap_quota=%ld, tx_buf_size=%ld, label=\"%s\"",
				        14*1024 + tx_buf_size, CAP_QUOTA, tx_buf_size, label)),
			Session_client(cap(), *tx_block_alloc, env.rm()),
			_max_block_count(_init_max_block_count(_tx.source()->bulk_buffer_size()))
		{ }

		/**
		 * Register handler for data-flow signals
		 *
		 * The handler is triggered on the arrival of new acknowledgements or
		 * when the server becomes ready for new requests. It is thereby able
		 * to execute 'update_jobs' on these conditions.
		 */
		void sigh(Genode::Signal_context_capability sigh)
		{
			tx_channel()->sigh_ack_avail(sigh);
			tx_channel()->sigh_ready_to_submit(sigh);
		}

		/**
		 * Handle the submission and completion of block-operation jobs
		 *
		 * \return  true if progress was made
		 */
		template <typename POLICY>
		bool update_jobs(POLICY &policy)
		{
			Tx::Source &tx = *_tx.source();

			bool overall_progress = false;

			for (;;) {

				/* track progress of a single iteration */
				bool progress = false;

				/* process acknowledgements */
				while (_try_process_ack(policy, tx))
					progress = true;

				/* try to submit pending requests */
				while (_try_submit_pending_job(policy, tx))
					progress = true;

				overall_progress |= progress;

				if (!progress)
					break;
			}

			if (overall_progress)
				tx.wakeup();

			return overall_progress;
		}

		/**
		 * Interface of 'POLICY' argument for 'update_jobs'
		 */
		struct Update_jobs_policy
		{
			/**
			 * Produce content for write operation
			 *
			 * \param offset  position of to-be-written data window in bytes
			 * \param dst     destination buffer (located within the I/O
			 *                communication buffer shared with the server)
			 * \param length  size of 'dst' buffer in bytes
			 */
			void produce_write_content(Job &, off_t offset,
			                           char *dst, size_t length);

			/**
			 * Consume data resulting from read operation
			 *
			 * \param offset  position of received data window in bytes
			 * \param src     pointer to received data
			 * \param length  number of bytes received
			 */
			void consume_read_result(Job &, off_t offset,
			                         char const *src, size_t length);

			/**
			 * Respond on the completion of the given job
			 */
			void completed(Job &, bool success);
		};

		/**
		 * Call 'fn' with each job as argument
		 *
		 * This method is intended for the destruction of the jobs associated
		 * with the connection before destructing the 'Connection' object.
		 */
		template <typename FN>
		void dissolve_all_jobs(FN const &fn)
		{
			_pending.dequeue_all([&] (Genode::Fifo_element<_JOB> &elem) {
				fn(elem.object()); });

			auto discard_tag_and_apply_fn = [&] (_JOB &job)
			{
				Job &job_base = job;
				job_base._tag.destruct();
				fn(job);
			};

			while (_tags.template apply_any<_JOB>(discard_tag_and_apply_fn));
		}
};


template <typename JOB>
template <typename POLICY>
bool Block::Connection<JOB>::_try_process_ack(POLICY &policy, Tx::Source &tx)
{
	/*
	 * Tx must be ready to accept at least one submission.
	 * This is needed to continue split read/write operations.
	 */
	if (!tx.ack_avail() || !tx.ready_to_submit())
		return false;

	Packet_descriptor const p = tx.try_get_acked_packet();

	Operation::Type const type = p.operation_type();

	bool release_packet = true;

	typename Tag_id_space::Id const id { p.tag().value };

	try {
		_tags.template apply<_JOB>(id, [&] (_JOB &job) {

			if (type == Operation::Type::READ)
				Job::_with_offset_and_length(job, [&] (off_t offset, size_t length) {
					policy.consume_read_result(job, offset,
					                           tx.packet_content(p), length); });

			/* needed to access private members of 'Job' (friend) */
			Job &job_base = job;

			bool const partial_read_or_write =
				p.succeeded() &&
				Operation::has_payload(type) &&
				(job_base._position + p.block_count() < job_base._operation.count);

			if (partial_read_or_write) {

				/*
				 * Issue next part of split read/write operation while reusing the
				 * existing payload allocation within the packet stream.
				 */
				job_base._position += p.block_count();
				job_base._submit(policy, job, tx);

				release_packet = false;

			} else {

				job_base._completed = true;
				job_base._tag.destruct();

				policy.completed(job, p.succeeded());
			}
		});
	}
	catch (typename Tag_id_space::Unknown_id) {
		Genode::warning("spurious block-operation acknowledgement");
	}

	if (release_packet)
		tx.release_packet(p);

	return true;
}


template <typename JOB>
template <typename POLICY>
bool Block::Connection<JOB>::_try_submit_pending_job(POLICY &policy, Tx::Source &tx)
{
	if (_pending.empty())
		return false;

	/*
	 * Preserve one slot for the submission, plus another slot to satisfy the
	 * precondition of '_try_process_ack'.
	 */
	if (!tx.ready_to_submit(2))
		return false;

	/*
	 * Allocate space for the payload in the packet-stream buffer.
	 */

	Payload payload { };
	try {
		_pending.head([&] (Genode::Fifo_element<_JOB> const &elem) {

			Job const &job = elem.object();

			if (!Operation::has_payload(job._operation.type))
				return;

			size_t const bytes = _info.block_size * job._curr_operation().count;

			payload = { .offset = tx.alloc_packet(bytes, _info.align_log2).offset(),
			            .bytes  = bytes };
		});
	}
	catch (Tx::Source::Packet_alloc_failed) {

		/* the packet-stream buffer is saturated */
		return false;
	}

	/*
	 * All preconditions for the submission of the job are satisfied.
	 * So the job can go from the pending to the in-progress stage.
	 */

	_pending.dequeue([&] (Genode::Fifo_element<_JOB> &elem) {

		Job &job = elem.object();

		/* let the job join the tag ID space, allocating a tag */
		job._tag.construct(elem.object(), _tags);

		job._payload = payload;
		job._submit(policy, elem.object(), tx);
	});

	return true;
}

#endif /* _INCLUDE__BLOCK_SESSION__CONNECTION_H_ */

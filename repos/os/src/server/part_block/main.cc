/*
 * \brief  Front end of the partition server
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <block_session/rpc_object.h>
#include <block/request_stream.h>
#include <os/session_policy.h>
#include <util/bit_allocator.h>

#include "gpt.h"
#include "mbr.h"

namespace Block {
	class  Session_component;
	struct Session_handler;
	struct Dispatch;
	class  Main;

	template <unsigned ITEMS> struct Job_queue;

	typedef Constructible<Job> Job_object;
	using   Response = Request_stream::Response;
};


template <unsigned ITEMS>
struct Block::Job_queue
{
	Job_object           _jobs[ITEMS];
	Bit_allocator<ITEMS> _alloc;

	addr_t alloc()
	{
		addr_t index = _alloc.alloc();
		return index;
	}

	void free(addr_t index)
	{
		if (_jobs[index].constructed())
			_jobs[index].destruct();

		_alloc.free(index);
	}

	template<typename FN>
	void with_job(addr_t index, FN const &fn)
	{
		fn(_jobs[index]);
	}
};


struct Block::Dispatch : Interface
{
	virtual Response submit(long number, Request const &request, addr_t addr) = 0;
	virtual void     update() = 0;
	virtual void     acknowledge_completed(bool all = true, long number = -1) = 0;
	virtual Response sync(long number, Request const &request) = 0;
};


struct Block::Session_handler : Interface
{
	Env                   &env;
	Attached_ram_dataspace ds;

	Signal_handler<Session_handler> request_handler
	  { env.ep(), *this, &Session_handler::handle };

	Session_handler(Env &env, size_t buffer_size)
	: env(env), ds(env.ram(), env.rm(), buffer_size)
	{ }

	virtual void handle_requests()= 0;

	void handle()
	{
		handle_requests();
	}
};


class Block::Session_component : public Rpc_object<Block::Session>,
                                 public Session_handler,
                                 public Block::Request_stream
{
	private:

		long      _number;
		Dispatch &_dispatcher;

	public:

		bool syncing { false };

		Session_component(Env &env, long number, size_t buffer_size,
		                  Session::Info info, Dispatch &dispatcher)
		: Session_handler(env, buffer_size),
		  Request_stream(env.rm(), ds.cap(), env.ep(), request_handler, info),
		  _number(number), _dispatcher(dispatcher)
		{
			env.ep().manage(*this);
		}

		~Session_component()
		{
			env.ep().dissolve(*this);
		}

		Info info() const override { return Request_stream::info(); }

		Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }

		long number() const { return _number; }

		bool acknowledge(Request &request)
		{
			bool progress = false;
			try_acknowledge([&] (Ack &ack) {
				if (progress) return;
				ack.submit(request);
				progress = true;
			});

			return progress;
		}

		void handle_requests() override
		{
			while (true) {

				bool progress = false;

				/*
				 * Acknowledge any pending packets before sending new request to the
				 * controller.
				 */
				_dispatcher.acknowledge_completed(false, _number);

				with_requests([&] (Request request) {

					Response response = Response::RETRY;

					if (syncing) return response;

					/* only READ/WRITE requests, others are noops for now */
					if (request.operation.type == Operation::Type::TRIM ||
					    request.operation.type == Operation::Type::INVALID) {
						request.success = true;
						progress = true;
						return Response::REJECTED;
					}

					if (!info().writeable && request.operation.type == Operation::Type::WRITE) {
						progress = true;
						return Response::REJECTED;
					}

					if (request.operation.type == Operation::Type::SYNC) {
						response = _dispatcher.sync(_number, request);
						if (response == Response::ACCEPTED) syncing = true;
						return response;
					}

					with_payload([&] (Request_stream::Payload const &payload) {
						payload.with_content(request, [&] (void *addr, size_t) {
							response = _dispatcher.submit(_number, request, addr_t(addr));
						});
					});

					if (response != Response::RETRY)
						progress = true;

					return response;
				});

				if (progress == false) break;
			}

			_dispatcher.update();

			/* poke */
			wakeup_client_if_needed();
		}
};


class Block::Main : Rpc_object<Typed_root<Session>>,
                    Dispatch
{
	private:

		Partition_table & _table();

		Env &_env;

		Attached_rom_dataspace _config { _env, "config" };

		Heap     _heap     { _env.ram(), _env.rm() };
		Reporter _reporter { _env, "partitions" };

		Number_of_bytes const _io_buffer_size =
			_config.xml().attribute_value("io_buffer",
			                              Number_of_bytes(4*1024*1024));

		Allocator_avl           _block_alloc { &_heap };
		Block_connection        _block    { _env, &_block_alloc, _io_buffer_size };
		Io_signal_handler<Main> _io_sigh  { _env.ep(), *this, &Main::_handle_io };
		Mbr_partition_table     _mbr      { _env, _block, _heap, _reporter };
		Gpt                     _gpt      { _env, _block, _heap, _reporter };
		Partition_table        &_partition_table { _table() };

		enum { MAX_SESSIONS = 128 };
		Session_component   *_sessions[MAX_SESSIONS] { };
		Job_queue<128>       _job_queue { };
		Registry<Block::Job> _job_registry { };

		unsigned _wake_up_index { 0 };

		void _wakeup_clients()
		{
			bool     first      = true;
			unsigned next_index = 0;
			for (long i = 0; i < MAX_SESSIONS; i++) {

				unsigned index = (_wake_up_index + i) % MAX_SESSIONS;

				if (!_sessions[index]) continue;

				if (_sessions[index]->syncing) {
					bool in_flight = false;

					_job_registry.for_each([&] (Job &job) {
						if (in_flight || i == job.number) {
							Operation &op = job.request.operation;
							in_flight |= (op.type == Operation::Type::WRITE ||
							              op.type == Operation::Type::SYNC);
						}
					});

					if (in_flight == false) _sessions[index]->syncing = false;
					else continue;
				}

				if (first) {
					/* to be more fair, start at index + 1 next time */
					next_index  = (index + 1) % MAX_SESSIONS;
					first       = false;
				}

				_sessions[index]->handle_requests();
			}

			_wake_up_index = next_index;
		}

		void _handle_io()
		{
			update();
			acknowledge_completed();
			_wakeup_clients();
		}

		Main(Main const &);
		Main &operator = (Main const &);


	public:

		struct No_partion_table : Exception { };
		struct Ambiguous_tables : Exception { };
		struct Invalid_config   : Exception { };

		Main(Env &env) : _env(env)
		{
			_block.sigh(_io_sigh);

			/* announce at parent */
			env.parent().announce(env.ep().manage(*this));
		}


		/***********************
		 ** Session interface **
		 ***********************/

		Genode::Session_capability session(Root::Session_args const &args,
		                                   Affinity const &) override
		{
			long num = -1;
			bool writeable = false;

			Session_label const label = label_from_args(args.string());
			try {
				Session_policy policy(label, _config.xml());

				/* read partition attribute */
				num = policy.attribute_value("partition", -1L);

				/* sessions are not writeable by default */
				writeable = policy.attribute_value("writeable", false);

			} catch (Xml_node::Nonexistent_attribute) {
				error("policy does not define partition number for for '",
				      label, "'");
				throw Service_denied();
			} catch (Session_policy::No_policy_defined) {
				error("rejecting session request, no matching policy for '",
				      label, "'");
				throw Service_denied();
			}

			try {
				_partition_table.partition(num);
			}
			catch (...) {
				error("Partition ", num, " unavailable for '", label, "'");
				throw Service_denied();
			}

			if (num >= MAX_SESSIONS || _sessions[num]) {
				error("Partition ", num, " already in use or session limit reached for '",
				      label, "'");
				throw Service_denied();
			}

			Ram_quota const ram_quota = ram_quota_from_args(args.string());
			size_t tx_buf_size =
				Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

			if (!tx_buf_size)
				throw Service_denied();

			/* delete ram quota by the memory needed for the session */
			size_t session_size = max((size_t)4096,
			                          sizeof(Session_component));
			if (ram_quota.value < session_size)
				throw Insufficient_ram_quota();

			/*
			 * Check if donated ram quota suffices for both
			 * communication buffers. Also check both sizes separately
			 * to handle a possible overflow of the sum of both sizes.
			 */
			if (tx_buf_size > ram_quota.value - session_size) {
				error("insufficient 'ram_quota', got ", ram_quota, ", need ",
				     tx_buf_size + session_size);
				throw Insufficient_ram_quota();
			}

			Session::Info info {
				.block_size  = _block.info().block_size,
				.block_count = _partition_table.partition(num).sectors,
				.align_log2  = 0,
				.writeable   = writeable,
			};

			_sessions[num] = new (_heap) Session_component(_env, num, tx_buf_size,
			                                               info, *this);
			return _sessions[num]->cap();
		}

		void close(Genode::Session_capability cap) override
		{
			for (long number = 0; number < MAX_SESSIONS; number++) {
				if (!_sessions[number] || !(cap == _sessions[number]->cap()))
					continue;

				destroy(_heap, _sessions[number]);
				_sessions[number] = nullptr;

				break;
			}
		}

		void upgrade(Genode::Session_capability, Root::Upgrade_args const&) override { }


		/************************
		 ** Update_jobs_policy **
		 ************************/

		void consume_read_result(Job &job, off_t,
		                         char const *src, size_t length)
		{
			if (!_sessions[job.number]) return;

			memcpy((void *)(job.addr + job.offset), src, length);
			job.offset += length;
		}

		void produce_write_content(Job &job, off_t, char *dst, size_t length)
		{
			memcpy(dst, (void *)(job.addr + job.offset), length);
			job.offset += length;
		}

		void completed(Job &job, bool success)
		{
			job.request.success = success;
			job.completed       = true;
		}


		/**************
		 ** Dispatch **
		 **************/

		void update() override { _block.update_jobs(*this); }

		Response submit(long number, Request const &request, addr_t addr) override
		{
			Partition &partition = _partition_table.partition(number);
			block_number_t last  = request.operation.block_number + request.operation.count;

			if (last > partition.sectors)
				return Response::REJECTED;

			addr_t index = 0;
			try {
				index  = _job_queue.alloc();
			} catch (...) { return Response::RETRY; }

			_job_queue.with_job(index, [&](Job_object &job) {

				Operation op     = request.operation;
				op.block_number += partition.lba;

				job.construct(_block, op, _job_registry, index, number, request, addr);
			});

			return Response::ACCEPTED;
		}

		Response sync(long number, Request const &request) override
		{
			addr_t index = 0;
			try {
				index = _job_queue.alloc();
			} catch (...) { return Response::RETRY; }

			_job_queue.with_job(index, [&](Job_object &job) {
				job.construct(_block, request.operation, _job_registry,
				              index, number, request, 0);
			});

			return Response::ACCEPTED;
		}

		void acknowledge_completed(bool all = true, long number = -1) override
		{
			_job_registry.for_each([&] (Job &job) {
				if (!job.completed) return;

				addr_t index = job.index;

				/* free orphans */
				if (!_sessions[job.number]) {
					_job_queue.free(index);
					return;
				}

				if (!all && job.number != number)
					return;

				if (_sessions[job.number]->acknowledge(job.request))
					_job_queue.free(index);
			});
		}
};


Block::Partition_table & Block::Main::_table()
{
	Xml_node const config = _config.xml();

	bool const ignore_gpt = config.attribute_value("ignore_gpt", false);
	bool const ignore_mbr = config.attribute_value("ignore_mbr", false);

	bool valid_mbr  = false;
	bool valid_gpt  = false;
	bool pmbr_found = false;
	bool report     = false;

	if (ignore_gpt && ignore_mbr) {
		error("invalid configuration: cannot ignore GPT as well as MBR");
		throw Invalid_config();
	}

	try {
		report = _config.xml().sub_node("report").attribute_value
                         ("partitions", false);
		if (report)
			_reporter.enabled(true);
	} catch(...) {}

	/*
	 * Try to parse MBR as well as GPT first if not instructued
	 * to ignore either one of them.
	 */

	if (!ignore_mbr) {
		try { valid_mbr = _mbr.parse(); }
		catch (Mbr_partition_table::Protective_mbr_found) {
			pmbr_found = true;
		} catch (...) { };
	}

	if (!ignore_gpt) {
		try { valid_gpt = _gpt.parse(); }
		catch (...) { }
	}

	/*
	 * Both tables are valid (although we would have expected a PMBR in
	 * conjunction with a GPT header - hybrid operation is not supported)
	 * and we will not decide which one to use, it is up to the user.
	 */
	if (valid_mbr && valid_gpt) {
		error("ambigious tables: found valid MBR as well as valid GPT");
		throw Ambiguous_tables();
	}

	if (valid_gpt && !pmbr_found) {
		warning("will use GPT without proper protective MBR");
	}

	/* PMBR missing, i.e, MBR part[0] contains whole disk and GPT valid */
	if (pmbr_found && ignore_gpt) {
		warning("found protective MBR but GPT is to be ignored");
	}

	/*
	 * Return the appropriate table or abort if none is found.
	 */

	if (valid_gpt) return _gpt;
	if (valid_mbr) return _mbr;

	error("Aborting: no partition table found.");
	throw No_partion_table();
}

void Component::construct(Genode::Env &env) { static Block::Main main(env); }

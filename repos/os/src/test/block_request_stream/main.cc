/*
 * \brief  Example block service
 * \author Norman Feske
 * \date   2018-12-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <block/request_stream.h>
#include <base/component.h>
#include <base/attached_ram_dataspace.h>
#include <root/root.h>

namespace Test {

	struct Block_session_component;
	template <unsigned> struct Jobs;

	struct Main;

	using namespace Genode;
}


struct Test::Block_session_component : Rpc_object<Block::Session>,
                                       private Block::Request_stream
{
	Entrypoint &_ep;

	static constexpr size_t BLOCK_SIZE = 4096;
	static constexpr size_t NUM_BLOCKS = 16;

	using Block::Request_stream::with_requests;
	using Block::Request_stream::with_content;
	using Block::Request_stream::try_acknowledge;
	using Block::Request_stream::wakeup_client_if_needed;

	Block_session_component(Region_map               &rm,
	                        Dataspace_capability      ds,
	                        Entrypoint               &ep,
	                        Signal_context_capability sigh)
	:
		Request_stream(rm, ds, ep, sigh,
		               Info { .block_size  = BLOCK_SIZE,
		                      .block_count = NUM_BLOCKS,
		                      .align_log2  = log2(BLOCK_SIZE),
		                      .writeable   = true }),
		_ep(ep)
	{
		_ep.manage(*this);
	}

	~Block_session_component() { _ep.dissolve(*this); }

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }
};


template <unsigned N>
struct Test::Jobs : Noncopyable
{
	Jobs() { }

	struct Entry
	{
		Block::Request request;

		enum State { UNUSED, IN_PROGRESS, COMPLETE } state;
	};

	Entry _entries[N] { };

	bool acceptable(Block::Request) const
	{
		for (unsigned i = 0; i < N; i++)
			if (_entries[i].state == Entry::UNUSED)
				return true;

		return false;
	}

	void submit(Block::Request request)
	{
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::UNUSED) {
				_entries[i] = Entry { .request = request,
				                      .state   = Entry::IN_PROGRESS };
				return;
			}
		}

		error("failed to accept request");
	}

	bool execute()
	{
		bool progress = false;
		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::IN_PROGRESS) {
				_entries[i].state = Entry::COMPLETE;
				_entries[i].request.success = true;
				progress = true;
			}
		}

		return progress;
	}

	void completed_job(Block::Request &out)
	{
		out = Block::Request { };

		for (unsigned i = 0; i < N; i++) {
			if (_entries[i].state == Entry::COMPLETE) {
				out = _entries[i].request;
				_entries[i].state = Entry::UNUSED;
				return;
			}
		}
	}

	/**
	 * Apply 'fn' with completed job, reset job
	 */
	template <typename FN>
	void with_any_completed_job(FN const &fn)
	{
		Block::Request request { };

		completed_job(request);

		if (request.operation.valid())
			fn(request);
	}
};


struct Test::Main : Rpc_object<Typed_root<Block::Session> >
{
	Env &_env;

	Constructible<Attached_ram_dataspace> _block_ds { };

	Constructible<Block_session_component> _block_session { };

	Signal_handler<Main> _request_handler { _env.ep(), *this, &Main::_handle_requests };

	Jobs<10> _jobs { };

	void _handle_requests()
	{
		if (!_block_session.constructed())
			return;

		Block_session_component &block_session = *_block_session;

		for (;;) {

			bool progress = false;

			/* import new requests */
			block_session.with_requests([&] (Block::Request request) {

				if (!_jobs.acceptable(request))
					return Block::Request_stream::Response::RETRY;

				/* access content of the request */
				block_session.with_content(request, [&] (void *ptr, size_t size) {
					(void)ptr;
					(void)size;
				});

				_jobs.submit(request);

				progress = true;

				return Block::Request_stream::Response::ACCEPTED;
			});

			/* process I/O */
			progress |= _jobs.execute();

			/* acknowledge finished jobs */
			block_session.try_acknowledge([&] (Block::Request_stream::Ack &ack) {

				_jobs.with_any_completed_job([&] (Block::Request request) {
					progress |= true;
					ack.submit(request);
				});
			});

			if (!progress)
				break;
		}

		block_session.wakeup_client_if_needed();
	}


	/*
	 * Root interface
	 */

	Capability<Session> session(Root::Session_args const &args,
	                            Affinity const &) override
	{
		log("new block session: ", args.string());

		size_t const ds_size =
			Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		if (ds_size >= ram_quota.value) {
			warning("communication buffer size exceeds session quota");
			throw Insufficient_ram_quota();
		}

		_block_ds.construct(_env.ram(), _env.rm(), ds_size);
		_block_session.construct(_env.rm(), _block_ds->cap(), _env.ep(),
		                         _request_handler);

		return _block_session->cap();
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const &) override { }

	void close(Capability<Session>) override
	{
		_block_session.destruct();
		_block_ds.destruct();
	}

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(*this));
	}
};


void Component::construct(Genode::Env &env) { static Test::Main inst(env); }

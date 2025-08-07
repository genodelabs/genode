/*
 * \brief  Genode block service provider C-API
 * \author Stefan Kalkowski
 * \date   2021-07-10
 */

/*
 * Copyright (C) 2006-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <base/node.h>
#include <block/request_stream.h>
#include <block/session_map.h>
#include <root/component.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <util/bit_array.h>

#include <genode_c_api/block.h>

using namespace Genode;

namespace {
	using namespace Genode;

	using Session_space = Id_space<genode_block_session>;
	using Session_map   = Block::Session_map<>;

	struct Device_info {
		using Name = String<64>;

		Name                 const name;
		Block::Session::Info const info;

		Session_space _session_space { };
		Session_map   _session_map   { };

		Device_info(const char * name, Block::Session::Info info)
		: name(name), info(info) { }
	};

} /* anonymous namespace */


class genode_block_session : public Rpc_object<Block::Session>
{
	private:

		friend class Block_root;

		Session_space::Element const _elem;

		Device_info::Name const _device_name;

		bool _device_gone { false };

		enum { MAX_REQUESTS = 32 };

		struct Request {
			enum State { FREE, IN_FLIGHT, DONE };

			State                state    { FREE };
			genode_block_request dev_req  { 0, GENODE_BLOCK_UNAVAIL,
			                                0, 0, nullptr };
			Block::Request       peer_req {};
		};

		genode_shared_dataspace * _ds;
		Block::Request_stream     _rs;
		Request                   _requests[MAX_REQUESTS];

		template <typename FUNC>
		void _first_request(Request::State state, FUNC const &fn)
		{
			for (unsigned idx = 0; idx < MAX_REQUESTS; idx++) {
				if (_requests[idx].state == state) {
					fn(_requests[idx]);
					return;
				}
			}
		}

		template <typename FUNC>
		void _for_each_request(Request::State state, FUNC const &fn)
		{
			for (unsigned idx = 0; idx < MAX_REQUESTS; idx++) {
				if (_requests[idx].state == state)
					fn(_requests[idx]);
			}
		}

		/*
		 * Non_copyable
		 */
		genode_block_session(const genode_block_session&);
		genode_block_session & operator=(const genode_block_session&);

	public:

		genode_block_session(Session_space                 &space,
		                     uint16_t                       session_id_value,
		                     Env                           &env,
		                     Block::Constrained_view const &view,
		                     Device_info             const &device_info,
		                     Signal_context_capability     Cap,
		                     size_t                        Buffer_size);

		Info info() const override { return _rs.info(); }

		Capability<Tx> tx_cap() override { return _rs.tx_cap(); }

		genode_block_request * request();
		bool ack(genode_block_request * req, bool success);

		void notify_peers() { _rs.wakeup_client_if_needed(); }

		Session_space::Id session_id() const { return _elem.id(); }

		Device_info::Name const & device_name() const { return _device_name; }

		void mark_device_gone() { _device_gone = true; }
};


class Block_root : public Root_component<genode_block_session>
{
	private:

		enum { MAX_BLOCK_DEVICES = 32 };

		struct Session_info {
			using Name = String<64>;

			Name                   name;
			Block::Session::Info   info;
			genode_block_session * block_session { nullptr };

			Session_info(const char * name, Block::Session::Info info)
			: name(name), info(info) {}
		};

		using Buffered_node = Genode::Buffered_node;

		Env                          &_env;
		Signal_context_capability     _sigh_cap;
		Constructible<Buffered_node>  _config   { };
		Expanding_reporter            _reporter { _env, "block_devices" };
		Constructible<Device_info>    _devices[MAX_BLOCK_DEVICES];
		bool                          _announced     { false };
		bool                          _report_needed { false };

		Block_root(const Block_root&);
		Block_root & operator=(const Block_root&);

		Create_result _create_session(const char *args, Affinity const &) override;

		void _destroy_session(genode_block_session &) override;

		template <typename FUNC>
		void _for_each_device_info(FUNC const &fn)
		{
			for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++)
				if (_devices[idx].constructed())
					if (fn(*_devices[idx]))
						break;
		}

		void _report();

	public:

		struct Invalid_block_device_id {};

		Block_root(Env &env, Allocator &alloc, Signal_context_capability);

		void announce_device(const char * name, Block::Session::Info info);
		void discontinue_device(const char * name);
		void for_each_session(const char * name, auto const & session_fn);
		void session_ack(const char * name, Session_space::Id id, auto const & session_fn);
		void notify_peers();
		void apply_config(Node const &);
};


static Block_root                           * _block_root        = nullptr;
static genode_shared_dataspace_alloc_attach_t _alloc_peer_buffer = nullptr;
static genode_shared_dataspace_free_t         _free_peer_buffer  = nullptr;


genode_block_request * genode_block_session::request()
{
	using Response = Block::Request_stream::Response;

	genode_block_request * ret = nullptr;

	_rs.with_requests([&] (Block::Request request) {

		if (_device_gone)
			return Response::REJECTED;

		if (ret)
			return Response::RETRY;

		/* ignored operations */
		if (request.operation.type == Block::Operation::Type::TRIM ||
		    request.operation.type == Block::Operation::Type::INVALID) {
			request.success = true;
			return Response::REJECTED;
		}

		Response response = Response::RETRY;

		_first_request(Request::FREE, [&] (Request &r) {

			r.state    = Request::IN_FLIGHT;
			r.peer_req = request;

			Block::Operation const op = request.operation;
			switch(op.type) {
			case Block::Operation::Type::SYNC:
				r.dev_req.op = GENODE_BLOCK_SYNC;
				break;
			case Block::Operation::Type::READ:
				r.dev_req.op = GENODE_BLOCK_READ;
				break;
			case Block::Operation::Type::WRITE:
				r.dev_req.op = GENODE_BLOCK_WRITE;
				break;
			default:
				r.dev_req.op = GENODE_BLOCK_UNAVAIL;
			};

			r.dev_req.id      = _elem.id().value;
			r.dev_req.blk_nr  = op.block_number;
			r.dev_req.blk_cnt = op.count;
			r.dev_req.addr    = (void*)
				(genode_shared_dataspace_local_address(_ds) + request.offset);

			ret = &r.dev_req;
			response = Response::ACCEPTED;
		});

		return response;
	});

	return ret;
}


bool genode_block_session::ack(genode_block_request * req, bool success)
{
	if (req->id != _elem.id().value)
		return false;

	bool result = false;
	_for_each_request(Request::IN_FLIGHT, [&] (Request &r) {
		if (&r.dev_req == req)
			r.state = Request::DONE;
	});

	/* Acknowledge any pending packets */
	_rs.try_acknowledge([&](Block::Request_stream::Ack &ack) {
		_first_request(Request::DONE, [&] (Request &r) {

			r.state = Request::FREE;
			r.peer_req.success = success;
			ack.submit(r.peer_req);
			result = true;
		});
	});

	return result;
}


genode_block_session::genode_block_session(Session_space                   &space,
                                           uint16_t                         session_id_value,
                                           Env                             &env,
                                           Block::Constrained_view   const &view,
                                           Device_info               const &device_info,
                                           Signal_context_capability        cap,
                                           size_t                           buffer_size)
:
	_elem(*this, space, Session_space::Id { .value = session_id_value }),
	_device_name(device_info.name),
	_ds(_alloc_peer_buffer(buffer_size)),
	_rs(env.rm(), genode_shared_dataspace_capability(_ds), env.ep(), cap,
	    device_info.info, view)
{ }


Block_root::Create_result Block_root::_create_session(const char * args,
                                                      Affinity const &)
{
	if (!_config.constructed())
		throw Service_denied();

	Session_label const label = label_from_args(args);
	return with_matching_policy(label, *_config,
		[&] (Node const &policy) -> Create_result {

			Device_info::Name const device =
				policy.attribute_value("device", Device_info::Name());

			Ram_quota const ram_quota = ram_quota_from_args(args);
			size_t const tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			if (!tx_buf_size)
				return Create_error::DENIED;

			if (tx_buf_size > ram_quota.value) {
				error("insufficient 'ram_quota' from '", label, "',"
				      " got ", ram_quota, ", need ", tx_buf_size);
				return Create_error::INSUFFICIENT_RAM;
			}

			bool const writeable_policy =
				policy.attribute_value("writeable", false);

			genode_block_session * ret = nullptr;
			_for_each_device_info([&] (Device_info & di) {
				if (di.name != device)
					return false;

				Session_map::Index new_session_id { 0u };

				di._session_map.alloc().with_result(
					[&] (Session_map::Alloc_ok ok) { new_session_id = ok.index; },
					[&] (Session_map::Alloc_error) { throw Service_denied(); });

				Block::Constrained_view view =
					Block::Constrained_view::from_args(args);
				view.writeable = di.info.writeable
				              && writeable_policy
				              && view.writeable;

				try {
					ret = new (md_alloc())
						genode_block_session(di._session_space, new_session_id.value,
						                     _env, view, di, _sigh_cap,
						                     tx_buf_size);
				} catch (...) {
					di._session_map.free(new_session_id); }

				return true;
			});

			if (!ret) return Create_error::DENIED;

			return *ret;
		},
		[] () -> Create_result { return Create_error::DENIED; });
}


void Block_root::_destroy_session(genode_block_session &session)
{
	genode_shared_dataspace * ds = session._ds;
	Session_space::Id const session_id = session.session_id();
	Device_info::Name const device_name = session.device_name();

	Genode::destroy(md_alloc(), &session);
	_free_peer_buffer(ds);

	_for_each_device_info([&] (Device_info &di) {
		if (di.name != device_name)
			return false;

		Session_map::Index const index =
			Session_map::Index::from_id(session_id.value);
		di._session_map.free(index);

		return true;
	});
}


void Block_root::_report()
{
	if (!_report_needed)
		return;

	_reporter.generate([&] (Generator &g) {
		_for_each_device_info([&] (Device_info &di) {
			g.node("device", [&] {
				g.attribute("label",       di.name);
				g.attribute("block_size",  di.info.block_size);
				g.attribute("block_count", di.info.block_count);
			});
			/* report all */
			return false;
		});
	});
}


void Block_root::announce_device(const char * name, Block::Session::Info info)
{
	for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++) {
		if (_devices[idx].constructed())
			continue;

		_devices[idx].construct(name, info);
		if (!_announced) {
			_env.parent().announce(_env.ep().manage(*this));
			_announced = true;
		}
		_report();
		return;
	}

	error("Could not announce driver for device ", name, ", no slot left!");
}


void Block_root::discontinue_device(const char * name)
{
	for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++) {
		if (!_devices[idx].constructed() || _devices[idx]->name != name)
			continue;

		Device_info &di = *_devices[idx];

		di._session_map.for_each_index([&] (Session_map::Index index) {
			Session_space::Id const session_id { .value = index.value };
			di._session_space.apply<genode_block_session>(session_id,
				[&] (genode_block_session & session) {
					session.mark_device_gone(); },
				[&] { });
			});

		_devices[idx].destruct();
		_report();
	}
}


void Block_root::session_ack(const char * name, Session_space::Id const id,
                             auto const & session_fn)
{
	_for_each_device_info([&] (Device_info &di) {
		if (di.name != name)
			return false;

		di._session_space.apply<genode_block_session>(id,
			[&] (genode_block_session & session) {
					session_fn(&session); },
			[&] { error("session ", id.value, " not found"); });

		return true;
	});
}


void Block_root::for_each_session(const char * name, auto const & session_fn)
{
	_for_each_device_info([&] (Device_info &di) {
		if (di.name != name)
			return false;

		di._session_map.for_each_index([&] (Session_map::Index index) {
			Session_space::Id const session_id { .value = index.value };
			di._session_space.apply<genode_block_session>(session_id,
				[&] (genode_block_session & session) {
						session_fn(&session); },
				[&] { error("session ", session_id.value, " not found"); });
			});
		return true;
	});
}


void Block_root::notify_peers()
{
	_for_each_device_info([&] (Device_info &di) {
		di._session_space.for_each<genode_block_session>(
			[&] (genode_block_session & session) {
				session.notify_peers(); });

		/* notify all peers */
		return false;
	});
}


void Block_root::apply_config(Node const &config)
{
	_config.construct(*md_alloc(), config);
	_report_needed = config.attribute_value("report", false);
}


Block_root::Block_root(Env &env, Allocator &alloc, Signal_context_capability cap)
:
	Root_component<genode_block_session>(env.ep(), alloc),
	_env(env), _sigh_cap(cap) { }


extern "C" void
genode_block_init(genode_env                           * env_ptr,
                  genode_allocator                     * alloc_ptr,
                  genode_signal_handler                * sigh_ptr,
                  genode_shared_dataspace_alloc_attach_t alloc_func,
                  genode_shared_dataspace_free_t         free_func)
{
	static Block_root root(*static_cast<Env*>(env_ptr),
	                   *static_cast<Allocator*>(alloc_ptr),
	                   cap(sigh_ptr));
	_alloc_peer_buffer = alloc_func;
	_free_peer_buffer  = free_func;
	_block_root        = &root;

}


extern "C" void genode_block_announce_device(const char *       name,
                                             unsigned long long sectors,
                                             int                writeable)
{
	enum { SIZE_LOG2_512 = 9 };

	if (!_block_root)
		return;

	_block_root->announce_device(name, { 1UL << SIZE_LOG2_512,
	                             sectors, SIZE_LOG2_512,
	                             (writeable != 0) ? true : false });
}


extern "C" void genode_block_discontinue_device(const char * name)
{
	if (_block_root)
		_block_root->discontinue_device(name);
}


extern "C" int
genode_block_session_for_each_by_name(const char * name,
                                      genode_block_session_context *ctx,
                                      genode_block_session_one_session_t session_fn)
{
	int finished = 1;

	if (!_block_root)
		return finished;

	_block_root->for_each_session(name,
		[&] (genode_block_session * session) {
			if (!session_fn(ctx, session))
				finished = 0;
		});

	return finished;
}


extern "C" void
genode_block_session_ack_by_name(const char *name, unsigned long id,
                                 genode_block_session_context *ctx,
                                 genode_block_session_one_session_t session_fn)
{
	if (!_block_root)
		return;

	_block_root->session_ack(name,
	                         Session_space::Id { .value = id },
		[&] (genode_block_session * session) {
			session_fn(ctx, session); });
}


extern "C" struct genode_block_request *
genode_block_request_by_session(struct genode_block_session * session)
{
	return session ? session->request() : nullptr;
}


extern "C" int genode_block_ack_request(struct genode_block_session * session,
                                        struct genode_block_request * req,
                                        int success)
{
	return session ? session->ack(req, success ? true : false)
	               : 0;
}


extern "C" void genode_block_notify_peers()
{
	if (_block_root) _block_root->notify_peers();
}


void genode_block_apply_config(Node const &config)
{
	if (_block_root) _block_root->apply_config(config);
}

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
#include <block/request_stream.h>
#include <root/component.h>
#include <os/buffered_xml.h>
#include <os/reporter.h>
#include <os/session_policy.h>

#include <genode_c_api/block.h>

using namespace Genode;


class genode_block_session : public Rpc_object<Block::Session>
{
	private:

		friend class Root;

		enum { MAX_REQUESTS = 32 };

		struct Request {
			enum State { FREE, IN_FLY, DONE };

			State                state    { FREE };
			genode_block_request dev_req  { GENODE_BLOCK_UNAVAIL,
			                                0, 0, nullptr };
			Block::Request       peer_req {};
		};

		genode_shared_dataspace * _ds;
		Block::Request_stream     _rs;
		Request                   _requests[MAX_REQUESTS];

		template <typename FUNC>
		void _first_request(Request::State state, FUNC const & fn)
		{
			for (unsigned idx = 0; idx < MAX_REQUESTS; idx++) {
				if (_requests[idx].state == state) {
					fn(_requests[idx]);
					return;
				}
			}
		}

		template <typename FUNC>
		void _for_each_request(Request::State state, FUNC const & fn)
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

		genode_block_session(Env                     & env,
		                     Block::Session::Info      info,
		                     Signal_context_capability cap,
		                     size_t                    buffer_size);

		Info info() const override { return _rs.info(); }

		Capability<Tx> tx_cap() override { return _rs.tx_cap(); }

		genode_block_request * request();
		void ack(genode_block_request * req, bool success);

		void notify_peers() { _rs.wakeup_client_if_needed(); }
};


class Root : public Root_component<genode_block_session>
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

		Env                         & _env;
		Signal_context_capability     _sigh_cap;
		Constructible<Buffered_xml>   _config   { };
		Reporter                      _reporter { _env, "block_devices" };
		Constructible<Session_info>   _sessions[MAX_BLOCK_DEVICES];
		bool                          _announced     { false };
		bool                          _report_needed { false };

		Root(const Root&);
		Root & operator=(const Root&);

		genode_block_session * _create_session(const char * args,
		                                       Affinity const &) override;

		void _destroy_session(genode_block_session * session) override;

		template <typename FUNC>
		void _for_each_session_info(FUNC const & fn)
		{
			for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++)
				if (_sessions[idx].constructed())
					fn(*_sessions[idx]);
		}

		void _report();

	public:

		struct Invalid_block_device_id {};

		Root(Env & env, Allocator & alloc, Signal_context_capability);

		void announce_device(const char * name, Block::Session::Info info);
		void discontinue_device(const char * name);
		genode_block_session * session(const char * name);
		void notify_peers();
		void apply_config(Xml_node const &);
};


static ::Root                               * _block_root        = nullptr;
static genode_shared_dataspace_alloc_attach_t _alloc_peer_buffer = nullptr;
static genode_shared_dataspace_free_t         _free_peer_buffer  = nullptr;


genode_block_request * genode_block_session::request()
{
	using Response = Block::Request_stream::Response;

	genode_block_request * ret = nullptr;

	_rs.with_requests([&] (Block::Request request) {

		if (ret)
			return Response::RETRY;

		/* ignored operations */
		if (request.operation.type == Block::Operation::Type::TRIM ||
		    request.operation.type == Block::Operation::Type::INVALID) {
			request.success = true;
			return Response::REJECTED;
		}

		Response response = Response::RETRY;

		_first_request(Request::FREE, [&] (Request & r) {

			r.state    = Request::IN_FLY;
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


void genode_block_session::ack(genode_block_request * req, bool success)
{
	_for_each_request(Request::IN_FLY, [&] (Request & r) {
		if (&r.dev_req == req)
			r.state = Request::DONE;
	});

	/* Acknowledge any pending packets */
	_rs.try_acknowledge([&](Block::Request_stream::Ack & ack) {
		_first_request(Request::DONE, [&] (Request & r) {
			r.state = Request::FREE;
			r.peer_req.success = success;
			ack.submit(r.peer_req);
		});
	});
}


genode_block_session::genode_block_session(Env                     & env,
                                           Block::Session::Info      info,
                                           Signal_context_capability cap,
                                           size_t                    buffer_size)
:
	_ds(_alloc_peer_buffer(buffer_size)),
	_rs(env.rm(), genode_shared_dataspace_capability(_ds), env.ep(), cap, info) { }


genode_block_session * ::Root::_create_session(const char * args,
                                                    Affinity const &)
{
	if (!_config.constructed())
		throw Service_denied();

	Session_label      const label = label_from_args(args);
	Session_policy     const policy(label, _config->xml());
	Session_info::Name const device =
		policy.attribute_value("device", Session_info::Name());

	Ram_quota const ram_quota = ram_quota_from_args(args);
	size_t const tx_buf_size =
		Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

	if (!tx_buf_size)
		throw Service_denied();

	if (tx_buf_size > ram_quota.value) {
		error("insufficient 'ram_quota' from '", label, "',"
		      " got ", ram_quota, ", need ", tx_buf_size);
		throw Insufficient_ram_quota();
	}

	genode_block_session * ret = nullptr;

	_for_each_session_info([&] (Session_info & si) {
		if (si.block_session || si.name != device)
			return;
		ret = new (md_alloc())
			genode_block_session(_env, si.info, _sigh_cap, tx_buf_size);
		si.block_session = ret;
	});

	if (!ret) throw Service_denied();
	return ret;
}


void ::Root::_destroy_session(genode_block_session * session)
{
	_for_each_session_info([&] (Session_info & si) {
		if (si.block_session == session)
			si.block_session = nullptr;
	});

	genode_shared_dataspace * ds = session->_ds;
	Genode::destroy(md_alloc(), session);
	_free_peer_buffer(ds);
}


void ::Root::_report()
{
	if (!_report_needed)
		return;

	_reporter.enabled(true);
	Reporter::Xml_generator xml(_reporter, [&] () {
		_for_each_session_info([&] (Session_info & si) {
			xml.node("device", [&] {
				xml.attribute("label",       si.name);
				xml.attribute("block_size",  si.info.block_size);
				xml.attribute("block_count", si.info.block_count);
			});
		});
	});
}


void ::Root::announce_device(const char * name, Block::Session::Info info)
{
	for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++) {
		if (_sessions[idx].constructed())
			continue;

		_sessions[idx].construct(name, info);
		if (!_announced) {
			_env.parent().announce(_env.ep().manage(*this));
			_announced = true;
		}
		_report();
		return;
	}

	error("Could not announce driver for device ", name, ", no slot left!");
}


void ::Root::discontinue_device(const char * name)
{
	for (unsigned idx = 0; idx < MAX_BLOCK_DEVICES; idx++) {
		if (!_sessions[idx].constructed() || _sessions[idx]->name != name)
			continue;

		_sessions[idx].destruct();
		_report();
		return;
	}
}


genode_block_session * ::Root::session(const char * name)
{
	genode_block_session * ret = nullptr;
	_for_each_session_info([&] (Session_info & si) {
		if (si.name == name)
			ret = si.block_session;
	});
	return ret;
}


void ::Root::notify_peers()
{
	_for_each_session_info([&] (Session_info & si) {
		if (si.block_session)
			si.block_session->notify_peers();
	});
}


void ::Root::apply_config(Xml_node const & config)
{
	_config.construct(*md_alloc(), config);
	_report_needed = config.attribute_value("report", false);
}


::Root::Root(Env & env, Allocator & alloc, Signal_context_capability cap)
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
	static ::Root root(*static_cast<Env*>(env_ptr),
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


extern "C" struct genode_block_session *
genode_block_session_by_name(const char * name)
{
	return _block_root ? _block_root->session(name) : nullptr;
}


extern "C" struct genode_block_request *
genode_block_request_by_session(struct genode_block_session * session)
{
	return session ? session->request() : nullptr;
}


extern "C" void genode_block_ack_request(struct genode_block_session * session,
                                         struct genode_block_request * req,
                                         int success)
{
	if (session)
		session->ack(req, success ? true : false);
}


extern "C" void genode_block_notify_peers()
{
	if (_block_root) _block_root->notify_peers();
}


void genode_block_apply_config(Xml_node const & config)
{
	if (_block_root) _block_root->apply_config(config);
}

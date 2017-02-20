/*
 * \brief  Child creation framework
 * \author Norman Feske
 * \date   2006-07-22
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/child.h>

using namespace Genode;


/***************
 ** Utilities **
 ***************/

namespace {

		/**
		 * Guard for transferring quota donation
		 *
		 * This class is used to provide transactional semantics of quota
		 * transfers. Establishing a new session involves several steps, in
		 * particular subsequent quota transfers. If one intermediate step
		 * fails, we need to revert all quota transfers that already took
		 * place. When instantated at a local scope, a 'Transfer' object guards
		 * a quota transfer. If the scope is left without prior an explicit
		 * acknowledgement of the transfer (for example via an exception), the
		 * destructor the 'Transfer' object reverts the transfer in flight.
		 */
		class Transfer {

			bool                   _ack;
			size_t                 _quantum;
			Ram_session_capability _from;
			Ram_session_capability _to;

			public:

				class Quota_exceeded : Exception { };

				/**
				 * Constructor
				 *
				 * \param quantim  number of bytes to transfer
				 * \param from     donator RAM session
				 * \param to       receiver RAM session
				 *
				 * \throw Quota_exceeded
				 */
				Transfer(size_t quantum,
				         Ram_session_capability from,
				         Ram_session_capability to)
				: _ack(false), _quantum(quantum), _from(from), _to(to)
				{
					if (_from.valid() && _to.valid() &&
					    Ram_session_client(_from).transfer_quota(_to, quantum)) {
						warning("not enough quota for a donation of ", quantum, " bytes");
						throw Quota_exceeded();
					}
				}

				/**
				 * Destructor
				 *
				 * The destructor will be called when leaving the scope of the
				 * 'session' function. If the scope is left because of an error
				 * (e.g., an exception), the donation will be reverted.
				 */
				~Transfer()
				{
					if (!_ack && _from.valid() && _to.valid())
						Ram_session_client(_to).transfer_quota(_from, _quantum);
				}

				/**
				 * Acknowledge quota donation
				 */
				void acknowledge() { _ack = true; }
		};
}


/***********
 ** Child **
 ***********/

template <typename SESSION>
static Service &parent_service()
{
	static Parent_service service(SESSION::service_name());
	return service;
}


void Child::yield(Resource_args const &args)
{
	Lock::Guard guard(_yield_request_lock);

	/* buffer yield request arguments to be picked up by the child */
	_yield_request_args = args;

	/* notify the child about the yield request */
	if (_yield_sigh.valid())
		Signal_transmitter(_yield_sigh).submit();
}


void Child::notify_resource_avail() const
{
	if (_resource_avail_sigh.valid())
		Signal_transmitter(_resource_avail_sigh).submit();
}


void Child::announce(Parent::Service_name const &name)
{
	if (!name.valid_string()) return;

	_policy.announce_service(name.string());
}


void Child::session_sigh(Signal_context_capability sigh)
{
	_session_sigh = sigh;

	if (!_session_sigh.valid())
		return;

	/*
	 * Deliver pending session response if a session became available before
	 * the signal handler got installed. This can happen for the very first
	 * asynchronously created session of a component. In 'component.cc', the
	 * signal handler is registered as response of the session request that
	 * needs asynchronous handling.
	 */
	_id_space.for_each<Session_state const>([&] (Session_state const &session) {

		if (session.phase == Session_state::AVAILABLE ||
		    session.phase == Session_state::QUOTA_EXCEEDED ||
		    session.phase == Session_state::INVALID_ARGS) {

			if (sigh.valid() && session.async_client_notify)
				Signal_transmitter(sigh).submit();
		}
	});
}


/**
 * Create session-state object for a dynamically created session
 *
 * \throw Parent::Quota_exceeded
 * \throw Parent::Service_denied
 */
Session_state &
create_session(Child_policy::Name const &child_name, Service &service,
               Session_label const &label,
               Session_state::Factory &factory, Id_space<Parent::Client> &id_space,
               Parent::Client::Id id, Session_state::Args const &args,
               Affinity const &affinity)
{
	try {
		return service.create_session(factory, id_space, id, label, args, affinity);
	}
	catch (Service::Quota_exceeded) {
		error(child_name, " requested session with insufficient session quota");
		throw Parent::Quota_exceeded();
	}
	catch (Allocator::Out_of_memory) {
		error("could not allocate session meta data for child ", child_name);
		throw Parent::Quota_exceeded();
	}
	catch (Id_space<Parent::Client>::Conflicting_id) {

		error(child_name, " requested conflicting session ID ", id, " "
		      "(service=", service.name(), " args=", args, ")");

		id_space.apply<Session_state>(id, [&] (Session_state &session) {
			error("existing session: ", session); });
	}
	throw Parent::Service_denied();
}


/*
 * \deprecated  Temporary wrapper around 'Child_policy::resolve_session_request'
 *              that tries both overloads.
 *
 * \throw Parent::Service_denied
 */
Child_policy::Route Child::_resolve_session_request(Child_policy &policy,
                                                    Service::Name const &name,
                                                    char const *argbuf)
{
	Session_label const label = label_from_args(argbuf);

	/*
	 * \deprecated  Try old interface, remove once all 'Child_policy'
	 *              implementations are updated.
	 */
	try {

		Session_state::Args args(argbuf);
		return Child_policy::Route {
			policy.resolve_session_request(name, args), label };
	}
	catch (Parent::Service_denied) { }

	return policy.resolve_session_request(name, label);
}


Session_capability Child::session(Parent::Client::Id id,
                                  Parent::Service_name const &name,
                                  Parent::Session_args const &args,
                                  Affinity             const &affinity)
{
	if (!name.valid_string() || !args.valid_string()) throw Unavailable();

	char argbuf[Parent::Session_args::MAX_SIZE];

	strncpy(argbuf, args.string(), sizeof(argbuf));

	/* prefix session label */
	Session_label const orig_label(label_from_args(argbuf));
	Arg_string::set_arg_string(argbuf, sizeof(argbuf), "label",
	                           prefixed_label(_policy.name(), orig_label).string());

	/* filter session arguments according to the child policy */
	_policy.filter_session_args(name.string(), argbuf, sizeof(argbuf));

	/* filter session affinity */
	Affinity const filtered_affinity = _policy.filter_session_affinity(affinity);

	size_t const ram_quota = Arg_string::find_arg(argbuf, "ram_quota").ulong_value(0);

	/* portion of quota to keep for ourself to maintain the session meta data */
	size_t const keep_ram_quota = _session_factory.session_costs();

	if (ram_quota < keep_ram_quota)
		throw Parent::Quota_exceeded();

	/* ram quota to be forwarded to the server */
	size_t const forward_ram_quota = ram_quota - keep_ram_quota;

	/* adjust the session information as presented to the server */
	Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota",
	                    forward_ram_quota);

	/* may throw a 'Parent::Service_denied' exception */
	Child_policy::Route route = _resolve_session_request(_policy, name.string(), argbuf);
	Service &service = route.service;

	Session_state &session =
		create_session(_policy.name(), service, route.label, _session_factory,
		               _id_space, id, argbuf, filtered_affinity);

	_policy.session_state_changed();

	session.ready_callback = this;
	session.closed_callback = this;

	try {
		/* transfer the quota donation from the child's account to ourself */
		Transfer donation_from_child(ram_quota, _ram.cap(), _policy.ref_ram_cap());

		/* transfer session quota from ourself to the service provider */
		Transfer donation_to_service(forward_ram_quota, _policy.ref_ram_cap(),
		                             service.ram());

		/* try to dispatch session request synchronously */
		service.initiate_request(session);

		if (session.phase == Session_state::INVALID_ARGS) {
			_revert_quota_and_destroy(session);
			throw Service_denied();
		}

		if (session.phase == Session_state::QUOTA_EXCEEDED) {
			_revert_quota_and_destroy(session);
			throw Parent::Quota_exceeded();
		}

		/* finish transaction */
		donation_from_child.acknowledge();
		donation_to_service.acknowledge();
	}
	catch (Transfer::Quota_exceeded) {
		/*
		 * Release session meta data if one of the quota transfers went wrong.
		 */
		session.destroy();
		throw Parent::Quota_exceeded();
	}

	/*
	 * Copy out the session cap before we are potentially kicking off the
	 * asynchonous request handling at the server to avoid doule-read
	 * issues with the session.cap, which will be asynchronously assigned
	 * by the server side.
	 */
	Session_capability cap = session.cap;

	/* if request was not handled synchronously, kick off async operation */
	if (session.phase == Session_state::CREATE_REQUESTED)
		service.wakeup();

	if (cap.valid())
		session.phase = Session_state::CAP_HANDED_OUT;

	return cap;
}


Session_capability Child::session_cap(Client::Id id)
{
	Session_capability cap;

	auto lamda = [&] (Session_state &session) {

		if (session.phase == Session_state::INVALID_ARGS
		 || session.phase == Session_state::QUOTA_EXCEEDED) {

			Session_state::Phase const phase = session.phase;

			/*
			 * Implicity discard the session request when delivering an
			 * exception because the exception will trigger the deallocation
			 * of the session ID at the child anyway.
			 */
			_revert_quota_and_destroy(session);

			if (phase == Session_state::INVALID_ARGS)
				throw Parent::Service_denied();
			else
				throw Parent::Quota_exceeded();
		}

		if (!session.alive())
			warning(_policy.name(), ": attempt to request cap for unavailable session: ", session);

		if (session.cap.valid())
			session.phase = Session_state::CAP_HANDED_OUT;

		cap = session.cap;
	};

	try {
		_id_space.apply<Session_state>(id, lamda); }

	catch (Id_space<Parent::Client>::Unknown_id) {
		warning(_policy.name(), " requested session cap for unknown ID"); }

	_policy.session_state_changed();
	return cap;
}


Parent::Upgrade_result Child::upgrade(Client::Id id, Parent::Upgrade_args const &args)
{
	if (!args.valid_string()) {
		warning("no valid session-upgrade arguments");
		return UPGRADE_DONE;
	}

	Upgrade_result result = UPGRADE_PENDING;

	_id_space.apply<Session_state>(id, [&] (Session_state &session) {

		if (session.phase != Session_state::CAP_HANDED_OUT) {
			warning("attempt to upgrade session in invalid state");
			return;
		}

		size_t const ram_quota =
			Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);

		try {
			/* transfer quota from client to ourself */
			Transfer donation_from_child(ram_quota, _ram.cap(), _policy.ref_ram_cap());

			/* transfer session quota from ourself to the service provider */
			Transfer donation_to_service(ram_quota, _policy.ref_ram_cap(),
			                             session.service().ram());

			session.increase_donated_quota(ram_quota);
			session.phase = Session_state::UPGRADE_REQUESTED;

			session.service().initiate_request(session);

			/* finish transaction */
			donation_from_child.acknowledge();
			donation_to_service.acknowledge();
		}
		catch (Transfer::Quota_exceeded) {
			warning(_policy.name(), ": upgrade of ", session.service().name(), " failed");
			throw Parent::Quota_exceeded();
		}

		if (session.phase == Session_state::CAP_HANDED_OUT) {
			result = UPGRADE_DONE;
			_policy.session_state_changed();
			return;
		}

		session.service().wakeup();
	});
	_policy.session_state_changed();
	return result;
}


void Child::_revert_quota_and_destroy(Session_state &session)
{
	try {
		/* transfer session quota from the service to ourself */
		Transfer donation_from_service(session.donated_ram_quota(),
		                               session.service().ram(), _policy.ref_ram_cap());

		/*
		 * Transfer session quota from ourself to the client (our child). In
		 * addition to the quota returned from the server, we also return the
		 * quota that we preserved for locally storing the session meta data
		 * ('session_costs').
		 */
		Transfer donation_to_client(session.donated_ram_quota() +
		                            _session_factory.session_costs(),
		                            _policy.ref_ram_cap(), ram_session_cap());
		/* finish transaction */
		donation_from_service.acknowledge();
		donation_to_client.acknowledge();
	}
	catch (Transfer::Quota_exceeded) {
		warning(_policy.name(), ": could not revert session quota (", session, ")"); }

	session.destroy();
	_policy.session_state_changed();
}


Child::Close_result Child::_close(Session_state &session)
{
	/*
	 * If session could not be established, destruct session immediately
	 * without involving the server
	 */
	if (session.phase == Session_state::INVALID_ARGS
	 || session.phase == Session_state::QUOTA_EXCEEDED) {
		_revert_quota_and_destroy(session);
		return CLOSE_DONE;
	}

	/* close session if alive */
	if (session.alive()) {
		session.phase = Session_state::CLOSE_REQUESTED;
		session.service().initiate_request(session);
	}

	/*
	 * The service may have completed the close request immediately (e.g.,
	 * a locally implemented service). In this case, we can skip the
	 * asynchonous handling.
	 */

	if (session.phase == Session_state::CLOSED) {
		_revert_quota_and_destroy(session);
		return CLOSE_DONE;
	}

	_policy.session_state_changed();

	session.discard_id_at_client();
	session.service().wakeup();

	return CLOSE_PENDING;
}


Child::Close_result Child::close(Client::Id id)
{
	/* refuse to close the child's initial sessions */
	if (Parent::Env::session_id(id))
		return CLOSE_DONE;

	try {
		Close_result result = CLOSE_PENDING;
		auto lamda = [&] (Session_state &session) { result = _close(session); };
		_id_space.apply<Session_state>(id, lamda);
		return result;
	}
	catch (Id_space<Parent::Client>::Unknown_id) { return CLOSE_DONE; }
}


void Child::session_ready(Session_state &session)
{
	if (_session_sigh.valid() && session.async_client_notify)
		Signal_transmitter(_session_sigh).submit();
}


void Child::session_closed(Session_state &session)
{
	/*
	 * If the session was provided by a child of us, 'service.ram()' returns
	 * the RAM session of the corresponding child. Since the session to the
	 * server is closed now, we expect the server to have released all donated
	 * resources so that we can decrease the servers' quota.
	 *
	 * If this goes wrong, the server is misbehaving.
	 */
	_revert_quota_and_destroy(session);

	if (_session_sigh.valid())
		Signal_transmitter(_session_sigh).submit();
}


void Child::session_response(Server::Id id, Session_response response)
{
	try {
		_policy.server_id_space().apply<Session_state>(id, [&] (Session_state &session) {

			switch (response) {

			case Parent::SESSION_CLOSED:
				session.phase = Session_state::CLOSED;
				if (session.closed_callback)
					session.closed_callback->session_closed(session);
				break;

			case Parent::INVALID_ARGS:
				session.phase = Session_state::INVALID_ARGS;
				if (session.ready_callback)
					session.ready_callback->session_ready(session);
				break;

			case Parent::QUOTA_EXCEEDED:
				session.phase = Session_state::QUOTA_EXCEEDED;
				if (session.ready_callback)
					session.ready_callback->session_ready(session);
				break;

			case Parent::SESSION_OK:
				if (session.phase == Session_state::UPGRADE_REQUESTED) {
					session.phase = Session_state::CAP_HANDED_OUT;
					if (session.ready_callback)
						session.ready_callback->session_ready(session);
				}
				break;
			}
		});
	} catch (Child_policy::Nonexistent_id_space) { }
}


void Child::deliver_session_cap(Server::Id id, Session_capability cap)
{
	try {
		_policy.server_id_space().apply<Session_state>(id, [&] (Session_state &session) {

			if (session.cap.valid()) {
				_error("attempt to assign session cap twice");
				return;
			}

			session.cap   = cap;
			session.phase = Session_state::AVAILABLE;

			if (session.ready_callback)
				session.ready_callback->session_ready(session);
		});
	} catch (Child_policy::Nonexistent_id_space) { }
}


void Child::exit(int exit_value)
{
	/*
	 * This function receives the hint from the child that now, its a good time
	 * to kill it. An inherited child class could use this hint to schedule the
	 * destruction of the child object.
	 *
	 * Note that the child object must not be destructed from by this function
	 * because it is executed by the thread contained in the child object.
	 */
	return _policy.exit(exit_value);
}


Thread_capability Child::main_thread_cap() const
{
	/*
	 * The '_initial_thread' is always constructed when this function is
	 * called because the RPC call originates from the active child.
	 */
	return _initial_thread.constructed() ? _initial_thread->cap()
	                                     : Thread_capability();
}


void Child::resource_avail_sigh(Signal_context_capability sigh)
{
	_resource_avail_sigh = sigh;
}


void Child::resource_request(Resource_args const &args)
{
	_policy.resource_request(args);
}


void Child::yield_sigh(Signal_context_capability sigh) { _yield_sigh = sigh; }


Parent::Resource_args Child::yield_request()
{
	Lock::Guard guard(_yield_request_lock);

	return _yield_request_args;
}


void Child::yield_response() { _policy.yield_response(); }


namespace {

	/**
	 * Return interface for interacting with the child's address space
	 *
	 * Depending on the return value of 'Child_policy::address_space', we
	 * either interact with a local object of via an RPC client stub.
	 */
	struct Child_address_space
	{
		Region_map_client _rm_client;
		Region_map       &_rm;

		Child_address_space(Pd_session &pd, Child_policy &policy)
		:
			_rm_client(pd.address_space()),
			_rm(policy.address_space(pd) ? *policy.address_space(pd) : _rm_client)
		{ }

		Region_map &region_map() { return _rm; }
	};
}


void Child::_try_construct_env_dependent_members()
{
	/* check if the environment sessions are complete */
	if (!_ram.cap().valid() || !_pd .cap().valid() ||
	    !_cpu.cap().valid() || !_log.cap().valid() || !_binary.cap().valid())
		return;

	/*
	 * If the ROM-session request for the dynamic linker was granted but the
	 * response to the session request is still outstanding, we have to wait.
	 * Note that we proceed if the session request was denied by the policy,
	 * which may be the case when using a statically linked executable.
	 */
	if (_linker.constructed() && !_linker->cap().valid())
		return;

	/*
	 * Mark all environment sessions as handed out to prevent the triggering
	 * of signals by 'Child::session_sigh' for these sessions.
	 */
	_id_space.for_each<Session_state>([&] (Session_state &session) {
		if (session.phase == Session_state::AVAILABLE)
			session.phase =  Session_state::CAP_HANDED_OUT; });

	/* call 'Child_policy::init' methods for the environment sessions */
	_policy.init(_ram.session(), _ram.cap());
	_policy.init(_cpu.session(), _cpu.cap());
	_policy.init(_pd.session(),  _pd.cap());

	try {
		_initial_thread.construct(_cpu.session(), _pd.cap(), "initial");
		_process.construct(_binary.session().dataspace(), _linker_dataspace(),
		                   _pd.cap(), _pd.session(), _ram.session(),
		                   *_initial_thread, _local_rm,
		                   Child_address_space(_pd.session(), _policy).region_map(),
		                   _parent_cap);
	}
	catch (Ram_session::Alloc_failed)           { _error("RAM allocation failed during ELF loading"); }
	catch (Cpu_session::Thread_creation_failed) { _error("unable to create initial thread"); }
	catch (Cpu_session::Out_of_metadata)        { _error("CPU session quota exhausted"); }
	catch (Process::Missing_dynamic_linker)     { _error("dynamic linker unavailable"); }
	catch (Process::Invalid_executable)         { _error("invalid ELF executable"); }
	catch (Region_map::Attach_failed)           { _error("ELF loading failed"); }
}


void Child::_discard_env_session(Id_space<Parent::Client>::Id id)
{
	auto discard_id_fn = [&] (Session_state &s) { s.discard_id_at_client(); };

	try { _id_space.apply<Session_state>(id, discard_id_fn); }
	catch (Id_space<Parent::Client>::Unknown_id) { }
}


void Child::initiate_env_ram_session() { _ram.initiate(); }


void Child::initiate_env_sessions()
{
	_pd    .initiate();
	_cpu   .initiate();
	_log   .initiate();
	_binary.initiate();

	/*
	 * Issue environment-session request for obtaining the linker binary. We
	 * accept this request to fail. In this case, the child creation may still
	 * succeed if the binary is statically linked.
	 */
	try {
		_linker.construct(*this, Parent::Env::linker(), _policy.linker_name());
		_linker->initiate();
	}
	catch (Parent::Service_denied) { }

	_try_construct_env_dependent_members();
}


Child::Child(Region_map      &local_rm,
             Rpc_entrypoint  &entrypoint,
             Child_policy    &policy)
:
	_policy(policy), _local_rm(local_rm), _entrypoint(entrypoint),
	_parent_cap(_entrypoint.manage(this))
{
	if (_policy.initiate_env_sessions()) {
		initiate_env_ram_session();
		initiate_env_sessions();
	}
}


Child::~Child()
{
	_entrypoint.dissolve(this);

	/*
	 * Purge the meta data about any dangling sessions provided by the child to
	 * other children.
	 *
	 * Note that the session quota is not transferred back to the respective
	 * clients.
	 *
	 * All the session meta data is lost after this point. In principle, we
	 * could accumulate the to-be-replenished quota at each client. Once the
	 * server is completely destroyed (and we thereby regained all of the
	 * server's resources, the RAM sessions of the clients could be updated.
	 * However, a client of a suddenly disappearing server is expected to be in
	 * trouble anyway and likely to get stuck on the next attempt to interact
	 * with the server. So the added complexity of reverting the session quotas
	 * would be to no benefit.
	 */
	try {
		auto lambda = [&] (Session_state &s) { _revert_quota_and_destroy(s); };
		while (_policy.server_id_space().apply_any<Session_state>(lambda));
	}
	catch (Child_policy::Nonexistent_id_space) { }

	/*
	 * Remove statically created env sessions from the child's ID space.
	 */
	_discard_env_session(Env::ram());
	_discard_env_session(Env::cpu());
	_discard_env_session(Env::pd());
	_discard_env_session(Env::log());

	/*
	 * Remove dynamically created sessions from the child's ID space.
	 */
	auto close_fn = [&] (Session_state &session) {
		session.closed_callback = nullptr;
		session.ready_callback  = nullptr;
		_close(session);
	};

	while (_id_space.apply_any<Session_state>(close_fn));

	/*
	 * Make sure to destroy the users of the child's environment sessions
	 * before destructing those sessions.
	 */
	_process.destruct();
	_initial_thread.destruct();
}


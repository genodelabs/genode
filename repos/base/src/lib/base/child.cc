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

/* Genode includes */
#include <base/child.h>
#include <base/quota_transfer.h>

/* base-internal includes */
#include <base/internal/child_policy.h>

using namespace Genode;


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
	Mutex::Guard guard(_yield_request_mutex);

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
		    session.phase == Session_state::INSUFFICIENT_RAM_QUOTA ||
		    session.phase == Session_state::INSUFFICIENT_CAP_QUOTA ||
		    session.phase == Session_state::SERVICE_DENIED) {

			if (sigh.valid() && session.async_client_notify)
				Signal_transmitter(sigh).submit();
		}
	});
}


/**
 * Create session-state object for a dynamically created session
 *
 * \throw Out_of_ram
 * \throw Out_of_caps
 * \throw Insufficient_cap_quota
 * \throw Insufficient_ram_quota
 * \throw Service_denied
 */
Session_state &
create_session(Child_policy::Name const &child_name, Service &service,
               Session_label const &label, Session::Diag diag,
               Session_state::Factory &factory, Id_space<Parent::Client> &id_space,
               Parent::Client::Id id, Session_state::Args const &args,
               Affinity const &affinity)
{
	try {
		return service.create_session(factory, id_space, id, label, diag, args, affinity); }

	catch (Insufficient_ram_quota) {
		error(child_name, " requested session with insufficient RAM quota");
		throw; }

	catch (Insufficient_cap_quota) {
		error(child_name, " requested session with insufficient cap quota");
		throw; }

	catch (Allocator::Out_of_memory) {
		error(child_name, " session meta data could not be allocated");
		throw Out_of_ram(); }

	catch (Id_space<Parent::Client>::Conflicting_id) {

		error(child_name, " requested conflicting session ID ", id, " "
		      "(service=", service.name(), " args=", args, ")");

		try {
			id_space.apply<Session_state>(id, [&] (Session_state &session) {
				error("existing session: ", session); });
		}
		catch (Id_space<Parent::Client>::Unknown_id) { }
	}
	throw Service_denied();
}


Session_capability Child::session(Parent::Client::Id id,
                                  Parent::Service_name const &name,
                                  Parent::Session_args const &args,
                                  Affinity             const &affinity)
{
	if (!name.valid_string() || !args.valid_string() || _pd.closed())
		throw Service_denied();

	char argbuf[Parent::Session_args::MAX_SIZE];

	copy_cstring(argbuf, args.string(), sizeof(argbuf));

	/* prefix session label */
	Session_label const label = prefixed_label(_policy.name(), label_from_args(argbuf));

	Arg_string::set_arg_string(argbuf, sizeof(argbuf), "label", label.string());

	/* filter session arguments according to the child policy */
	_policy.filter_session_args(name.string(), argbuf, sizeof(argbuf));

	/* filter session affinity */
	Affinity const filtered_affinity = _policy.filter_session_affinity(affinity);

	Cap_quota const cap_quota = cap_quota_from_args(argbuf);
	Ram_quota const ram_quota = ram_quota_from_args(argbuf);

	/* portion of quota to keep for ourself to maintain the session meta data */
	size_t const keep_ram_quota = _session_factory.session_costs();

	if (ram_quota.value < keep_ram_quota)
		throw Insufficient_ram_quota();

	/* ram quota to be forwarded to the server */
	Ram_quota const forward_ram_quota { ram_quota.value - keep_ram_quota };

	/* adjust the session information as presented to the server */
	Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", (int)forward_ram_quota.value);

	/* may throw a 'Service_denied' exception */
	Child_policy::Route route =
		_policy.resolve_session_request(name.string(), label,
		                                session_diag_from_args(argbuf));

	Service &service = route.service;

	/* propagate diag flag */
	Arg_string::set_arg(argbuf, sizeof(argbuf), "diag", route.diag.enabled);

	Session_state &session =
		create_session(_policy.name(), service, route.label, route.diag,
		               _session_factory, _id_space, id, argbuf, filtered_affinity);

	_policy.session_state_changed();

	session.ready_callback = this;
	session.closed_callback = this;

	try {
		Ram_transfer::Remote_account ref_ram_account { _policy.ref_pd(), _policy.ref_pd_cap() };
		Cap_transfer::Remote_account ref_cap_account { _policy.ref_pd(), _policy.ref_pd_cap()  };

		Ram_transfer::Remote_account ram_account { pd(), pd_session_cap() };
		Cap_transfer::Remote_account cap_account { pd(), pd_session_cap() };

		/* transfer the quota donation from the child's account to ourself */
		Ram_transfer ram_donation_from_child(ram_quota, ram_account, ref_ram_account);
		Cap_transfer cap_donation_from_child(cap_quota, cap_account, ref_cap_account);

		/* transfer session quota from ourself to the service provider */
		Ram_transfer ram_donation_to_service(forward_ram_quota, ref_ram_account, service);
		Cap_transfer cap_donation_to_service(cap_quota,         ref_cap_account, service);

		/* finish transaction */
		ram_donation_from_child.acknowledge();
		cap_donation_from_child.acknowledge();
		ram_donation_to_service.acknowledge();
		cap_donation_to_service.acknowledge();
	}
	/*
	 * Release session meta data if one of the quota transfers went wrong.
	 */
	catch (Ram_transfer::Quota_exceeded) {
		session.destroy();
		throw Out_of_ram();
	}
	catch (Cap_transfer::Quota_exceeded) {
		session.destroy();
		throw Out_of_caps();
	}

	/* try to dispatch session request synchronously */
	service.initiate_request(session);

	if (session.phase == Session_state::SERVICE_DENIED) {
		_revert_quota_and_destroy(session);
		throw Service_denied();
	}

	if (session.phase == Session_state::INSUFFICIENT_RAM_QUOTA) {
		_revert_quota_and_destroy(session);
		throw Insufficient_ram_quota();
	}

	if (session.phase == Session_state::INSUFFICIENT_CAP_QUOTA) {
		_revert_quota_and_destroy(session);
		throw Insufficient_cap_quota();
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

		if (session.phase == Session_state::SERVICE_DENIED
		 || session.phase == Session_state::INSUFFICIENT_RAM_QUOTA
		 || session.phase == Session_state::INSUFFICIENT_CAP_QUOTA) {

			Session_state::Phase const phase = session.phase;

			/*
			 * Implicity discard the session request when delivering an
			 * exception because the exception will trigger the deallocation
			 * of the session ID at the child anyway.
			 */
			_revert_quota_and_destroy(session);

			switch (phase) {
			case Session_state::SERVICE_DENIED:         throw Service_denied();
			case Session_state::INSUFFICIENT_RAM_QUOTA: throw Insufficient_ram_quota();
			case Session_state::INSUFFICIENT_CAP_QUOTA: throw Insufficient_cap_quota();
			default: break;
			}
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

	/* ignore suprious request that may arrive after 'close_all_sessions' */
	if (_pd.closed())
		return UPGRADE_PENDING;

	Upgrade_result result = UPGRADE_PENDING;

	auto upgrade_session = [&] (Session_state &session) {

		if (session.phase != Session_state::CAP_HANDED_OUT) {
			warning("attempt to upgrade session in invalid state");
			return;
		}

		Ram_quota const ram_quota {
			Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0) };

		Cap_quota const cap_quota {
			Arg_string::find_arg(args.string(), "cap_quota").ulong_value(0) };

		try {
			Ram_transfer::Remote_account ref_ram_account { _policy.ref_pd(), _policy.ref_pd_cap() };
			Cap_transfer::Remote_account ref_cap_account { _policy.ref_pd(), _policy.ref_pd_cap()  };

			Ram_transfer::Remote_account ram_account { pd(), pd_session_cap() };
			Cap_transfer::Remote_account cap_account { pd(), pd_session_cap() };

			/* transfer quota from client to ourself */
			Ram_transfer ram_donation_from_child(ram_quota, ram_account, ref_ram_account);
			Cap_transfer cap_donation_from_child(cap_quota, cap_account, ref_cap_account);

			/* transfer session quota from ourself to the service provider */
			Ram_transfer ram_donation_to_service(ram_quota, ref_ram_account, session.service());
			Cap_transfer cap_donation_to_service(cap_quota, ref_cap_account, session.service());

			session.increase_donated_quota(ram_quota, cap_quota);
			session.phase = Session_state::UPGRADE_REQUESTED;

			session.service().initiate_request(session);

			/* finish transaction */
			ram_donation_from_child.acknowledge();
			cap_donation_from_child.acknowledge();
			ram_donation_to_service.acknowledge();
			cap_donation_to_service.acknowledge();
		}
		catch (Ram_transfer::Quota_exceeded) {
			warning(_policy.name(), ": RAM upgrade of ", session.service().name(), " failed");
			throw Out_of_ram();
		}
		catch (Cap_transfer::Quota_exceeded) {
			warning(_policy.name(), ": cap upgrade of ", session.service().name(), " failed");
			throw Out_of_caps();
		}

		if (session.phase == Session_state::CAP_HANDED_OUT) {
			result = UPGRADE_DONE;
			_policy.session_state_changed();
			return;
		}

		session.service().wakeup();
	};

	try { _id_space.apply<Session_state>(id, upgrade_session); }
	catch (Id_space<Parent::Client>::Unknown_id) { }

	_policy.session_state_changed();
	return result;
}


void Child::_revert_quota_and_destroy(Session_state &session)
{
	Ram_transfer::Remote_account   ref_ram_account(_policy.ref_pd(), _policy.ref_pd_cap());
	Ram_transfer::Account     &service_ram_account = session.service();
	Ram_transfer::Remote_account child_ram_account(pd(), pd_session_cap());

	Cap_transfer::Remote_account   ref_cap_account(_policy.ref_pd(), _policy.ref_pd_cap());
	Cap_transfer::Account     &service_cap_account = session.service();
	Cap_transfer::Remote_account child_cap_account(pd(), pd_session_cap());

	try {
		/* transfer session quota from the service to ourself */
		Ram_transfer ram_donation_from_service(session.donated_ram_quota(),
		                                       service_ram_account, ref_ram_account);

		Cap_transfer cap_donation_from_service(session.donated_cap_quota(),
		                                       service_cap_account, ref_cap_account);

		/*
		 * Transfer session quota from ourself to the client (our child). In
		 * addition to the quota returned from the server, we also return the
		 * quota that we preserved for locally storing the session meta data
		 * ('session_costs').
		 */
		Ram_quota const returned_ram { session.donated_ram_quota().value +
		                               _session_factory.session_costs() };

		Ram_transfer ram_donation_to_client(returned_ram,
		                                    ref_ram_account, child_ram_account);
		Cap_transfer cap_donation_to_client(session.donated_cap_quota(),
		                                    ref_cap_account, child_cap_account);

		/* finish transaction */
		ram_donation_from_service.acknowledge();
		cap_donation_from_service.acknowledge();
		ram_donation_to_client.acknowledge();
		cap_donation_to_client.acknowledge();
	}
	catch (Ram_transfer::Quota_exceeded) {
		warning(_policy.name(), ": could not revert session RAM quota (", session, ")"); }
	catch (Cap_transfer::Quota_exceeded) {
		warning(_policy.name(), ": could not revert session cap quota (", session, ")"); }

	session.destroy();
	_policy.session_state_changed();
}


Child::Close_result Child::_close(Session_state &session)
{
	/*
	 * If session could not be established, destruct session immediately
	 * without involving the server
	 */
	if (session.phase == Session_state::SERVICE_DENIED
	 || session.phase == Session_state::INSUFFICIENT_RAM_QUOTA
	 || session.phase == Session_state::INSUFFICIENT_CAP_QUOTA) {
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

				/*
				 * If the client exists, reflect the response to the client
				 * via the 'closed_callback'. If the client has vanished,
				 * i.e., if the close request was issued by ourself while
				 * killing a child, we drop the session state immediately.
				 */
				if (session.closed_callback) {
					session.closed_callback->session_closed(session);

				} else {

					/*
					 * The client no longer exists. So we cannot take the
					 * regular path of executing '_revert_quota_and_destroy' in
					 * the context of the client. Instead, we immediately
					 * withdraw the session quota from the server ('this') to
					 * the reference account, and destroy the session object.
					 */
					Ram_transfer::Remote_account ref_ram_account(_policy.ref_pd(), _policy.ref_pd_cap());
					Ram_transfer::Account   &service_ram_account = session.service();

					Cap_transfer::Remote_account ref_cap_account(_policy.ref_pd(), _policy.ref_pd_cap());
					Cap_transfer::Account   &service_cap_account = session.service();

					try {
						/* transfer session quota from the service to ourself */
						Ram_transfer ram_donation_from_service(session.donated_ram_quota(),
						                                       service_ram_account, ref_ram_account);

						Cap_transfer cap_donation_from_service(session.donated_cap_quota(),
						                                       service_cap_account, ref_cap_account);

						ram_donation_from_service.acknowledge();
						cap_donation_from_service.acknowledge();
					}
					catch (Ram_transfer::Quota_exceeded) {
						warning(_policy.name(), " failed to return session RAM quota "
						        "(", session.donated_ram_quota(), ")"); }
					catch (Cap_transfer::Quota_exceeded) {
						warning(_policy.name(), " failed to return session cap quota "
						        "(", session.donated_cap_quota(), ")"); }

					session.destroy();
					_policy.session_state_changed();
				}
				break;

			case Parent::SERVICE_DENIED:
				session.phase = Session_state::SERVICE_DENIED;
				if (session.ready_callback)
					session.ready_callback->session_ready(session);
				break;

			case Parent::INSUFFICIENT_RAM_QUOTA:
				session.phase = Session_state::INSUFFICIENT_RAM_QUOTA;
				if (session.ready_callback)
					session.ready_callback->session_ready(session);
				break;

			case Parent::INSUFFICIENT_CAP_QUOTA:
				session.phase = Session_state::INSUFFICIENT_CAP_QUOTA;
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
	}
	catch (Child_policy::Nonexistent_id_space) { }
	catch (Id_space<Parent::Client>::Unknown_id) {
		warning("unexpected session response for unknown session"); }
}


void Child::deliver_session_cap(Server::Id id, Session_capability cap)
{
	try {
		_policy.server_id_space().apply<Session_state>(id, [&] (Session_state &session) {

			/* ignore responses after 'close_all_sessions' of the client */
			if (session.phase != Session_state::CREATE_REQUESTED)
				return;

			if (session.cap.valid()) {
				_error("attempt to assign session cap twice");
				return;
			}

			/*
			 * If the client vanished during the session creation, the
			 * session-close state change must be reflected to the server
			 * as soon as the session becomes available. This enables the
			 * server to wind down the session. If we just discarded the
			 * session, the server's ID space would become inconsistent
			 * with ours.
			 */
			if (!session.client_exists()) {
				session.phase = Session_state::CLOSE_REQUESTED;
				session.service().initiate_request(session);
				return;
			}

			session.cap   = cap;
			session.phase = Session_state::AVAILABLE;

			if (session.ready_callback)
				session.ready_callback->session_ready(session);
		});
	}
	catch (Child_policy::Nonexistent_id_space) { }
	catch (Id_space<Parent::Client>::Unknown_id) { }
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
	Mutex::Guard guard(_yield_request_mutex);

	return _yield_request_args;
}


void Child::yield_response() { _policy.yield_response(); }


void Child::heartbeat()
{
	/*
	 * Issue heartbeat requests not before the component has registered a
	 * handler
	 */
	if (!_heartbeat_sigh.valid())
		return;

	_outstanding_heartbeats++;

	Signal_transmitter(_heartbeat_sigh).submit();
}


unsigned Child::skipped_heartbeats() const
{
	/*
	 * An '_outstanding_heartbeats' value of 1 is fine because the child needs
	 * some time to respond to the heartbeat signal. However, at the time when
	 * the second (or later) heartbeat signal is triggered, the first one
	 * should have been answered.
	 */
	return (_outstanding_heartbeats > 1) ? _outstanding_heartbeats - 1 : 0;
}


void Child::heartbeat_sigh(Signal_context_capability sigh)
{
	_heartbeat_sigh = sigh;
}


void Child::heartbeat_response() { _outstanding_heartbeats = 0; }


void Child::_try_construct_env_dependent_members()
{
	/* check if the environment sessions are complete */
	if (!_pd.cap().valid() || !_cpu.cap().valid() || !_log.cap().valid()
	 || !_binary.cap().valid())
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

	if (_process.constructed())
		return;

	_policy.init(_cpu.session(), _cpu.cap());

	Process::Type const type = _policy.forked()
	                         ? Process::TYPE_FORKED : Process::TYPE_LOADED;
	try {
		_initial_thread.construct(_cpu.session(), _pd.cap(), _policy.name());
		_policy.with_address_space(_pd.session(), [&] (Region_map &address_space) {
			_process.construct(type, _linker_dataspace(), _pd.session(),
			                   *_initial_thread, _initial_thread_start,
			                   _local_rm, address_space, cap()); });
	}
	catch (Out_of_ram)                          { _error("out of RAM during ELF loading"); }
	catch (Out_of_caps)                         { _error("out of caps during ELF loading"); }
	catch (Cpu_session::Thread_creation_failed) { _error("unable to create initial thread"); }
	catch (Process::Missing_dynamic_linker)     { _error("dynamic linker unavailable"); }
	catch (Process::Invalid_executable)         { _error("invalid ELF executable"); }
	catch (Region_map::Invalid_dataspace)       { _error("ELF loading failed (Invalid_dataspace)"); }
	catch (Region_map::Region_conflict)         { _error("ELF loading failed (Region_conflict)"); }
}


void Child::_discard_env_session(Id_space<Parent::Client>::Id id)
{
	auto discard_id_fn = [&] (Session_state &s) { s.discard_id_at_client(); };

	try { _id_space.apply<Session_state>(id, discard_id_fn); }
	catch (Id_space<Parent::Client>::Unknown_id) { }
}


void Child::initiate_env_pd_session()
{
	_pd.initiate();
	_policy.init(_pd.session(), _pd.cap());
}


void Child::initiate_env_sessions()
{
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
	catch (Service_denied) { }

	_try_construct_env_dependent_members();
}


/**
 * Return any CPU session that is initiated by the child
 *
 * \return client ID of a CPU session, or
 *         client ID 0 if no session exists
 */
static Parent::Client::Id any_cpu_session_id(Id_space<Parent::Client> const &id_space)
{
	Parent::Client::Id result { 0 };
	id_space.for_each<Session_state const>([&] (Session_state const &session) {
		if (result.value)
			return;

		bool cpu = (session.service().name() == Cpu_session::service_name());
		bool env = Parent::Env::session_id(session.id_at_client());

		if (!env && cpu)
			result = session.id_at_client();
	});
	return result;
}


void Child::close_all_sessions()
{
	/*
	 * Destroy CPU sessions prior to other session types to avoid page-fault
	 * warnings generated by threads that are losing their PD while still
	 * running.
	 */
	while (unsigned long id_value = any_cpu_session_id(_id_space).value) {
		Close_result const close_result = close(Parent::Client::Id{id_value});

		/* break infinte loop if CPU session is provided by a child */
		if (close_result != CLOSE_DONE)
			break;
	}

	_initial_thread.destruct();

	if (KERNEL_SUPPORTS_EAGER_CHILD_DESTRUCTION)
		_cpu._connection.destruct();

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
	 * Issue close requests to the providers of the environment sessions,
	 * which may be async services.
	 */
	_log.close();
	_binary.close();
	if (_linker.constructed())
		_linker->close();
	_pd.close();

	/*
	 * Remove statically created env sessions from the child's ID space.
	 */
	_discard_env_session(Env::cpu());
	_discard_env_session(Env::pd());
	_discard_env_session(Env::log());
	_discard_env_session(Env::binary());
	_discard_env_session(Env::linker());

	/*
	 * Remove dynamically created sessions from the child's ID space.
	 */
	auto close_fn = [&] (Session_state &session) {
		session.closed_callback = nullptr;
		session.ready_callback  = nullptr;

		Close_result const close_result = _close(session);

		if (close_result == CLOSE_PENDING)
			session.discard_id_at_client();
	};

	while (_id_space.apply_any<Session_state>(close_fn));

	if (!KERNEL_SUPPORTS_EAGER_CHILD_DESTRUCTION)
		_cpu._connection.destruct();
}


Child::Child(Region_map      &local_rm,
             Rpc_entrypoint  &entrypoint,
             Child_policy    &policy)
:
	_policy(policy), _local_rm(local_rm), _parent_cap_guard(entrypoint, *this)
{
	if (_policy.initiate_env_sessions()) {
		initiate_env_pd_session();
		initiate_env_sessions();
	}
}


Child::~Child()
{
	close_all_sessions();
	_process.destruct();
}


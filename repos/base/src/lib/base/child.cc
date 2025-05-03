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
 * If successful, 'fn' is called with a new 'Session_state &' as argument.
 */
Parent::Session_result
with_new_session(Child_policy::Name const &child_name, Service &service,
                 Session_label const &label, Session::Diag diag,
                 Session_state::Factory &factory, Id_space<Parent::Client> &id_space,
                 Parent::Client::Id id, Session_state::Args const &args,
                 Affinity const &affinity,
                 auto const &fn)
{
	using Error = Parent::Session_error;

	Error session_error = Error::DENIED;
	try {
		return fn(service.create_session(factory, id_space, id, label, diag, args, affinity));
	}
	catch (Insufficient_ram_quota) { session_error = Error::INSUFFICIENT_RAM_QUOTA; }
	catch (Insufficient_cap_quota) { session_error = Error::INSUFFICIENT_CAP_QUOTA; }
	catch (Out_of_ram)             { session_error = Error::OUT_OF_RAM;  }
	catch (Out_of_caps)            { session_error = Error::OUT_OF_CAPS; }
	catch (Id_space<Parent::Client>::Conflicting_id) {

		error(child_name, " requested conflicting session ID ", id, " "
		      "(service=", service.name(), " args=", args, ")");

		id_space.apply<Session_state>(id,
			[&] (Session_state &session) { error("existing session: ", session); },
			[&] /* missing */ { });
	}

	if (session_error == Error::OUT_OF_RAM || session_error == Error::OUT_OF_CAPS)
		error(child_name, " session meta data could not be allocated");

	if (session_error == Error::INSUFFICIENT_RAM_QUOTA)
		error(child_name, " requested session with insufficient RAM quota");

	if (session_error == Error::INSUFFICIENT_CAP_QUOTA)
		error(child_name, " requested session with insufficient cap quota");

	return session_error;
}


Parent::Session_result Child::session(Parent::Client::Id id,
                                      Parent::Service_name const &name,
                                      Parent::Session_args const &args,
                                      Affinity             const &affinity)
{
	if (!name.valid_string() || !args.valid_string() || _pd.closed())
		return Session_error::DENIED;

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
		return Session_error::INSUFFICIENT_RAM_QUOTA;

	/* ram quota to be forwarded to the server */
	Ram_quota const forward_ram_quota { ram_quota.value - keep_ram_quota };

	/* adjust the session information as presented to the server */
	Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", (int)forward_ram_quota.value);

	auto create = [&] (Pd_session &pd, Child_policy::Route const &route)
	{
		Service &service = route.service;

		/* propagate diag flag */
		Arg_string::set_arg(argbuf, sizeof(argbuf), "diag", route.diag.enabled);

		return with_new_session(_policy.name(), service, route.label, route.diag,
		                       _session_factory, _id_space, id, argbuf, filtered_affinity,

			[&] (Session_state &session) -> Session_result {

				_policy.session_state_changed();

				session.ready_callback = this;
				session.closed_callback = this;

				try {
					Ram_transfer::Remote_account ref_ram_account { _policy.ref_account(), _policy.ref_account_cap() };
					Cap_transfer::Remote_account ref_cap_account { _policy.ref_account(), _policy.ref_account_cap() };

					Ram_transfer::Remote_account ram_account { pd, pd_session_cap() };
					Cap_transfer::Remote_account cap_account { pd, pd_session_cap() };

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
					return Session_error::OUT_OF_RAM;
				}
				catch (Cap_transfer::Quota_exceeded) {
					session.destroy();
					return Session_error::OUT_OF_CAPS;
				}

				/* try to dispatch session request synchronously */
				{
					using Error = Service::Initiate_error;
					auto session_error = [] (Error e)
					{
						switch (e) {
						case Error::OUT_OF_RAM:  return Session_error::OUT_OF_RAM;
						case Error::OUT_OF_CAPS: break;
						}
						return Session_error::OUT_OF_CAPS;
					};
					auto const result = service.initiate_request(session);
					if (result.failed())
						return result.convert<Session_error>(
							[&] (Ok) /* never */ { return Session_error { }; },
							[&] (Error e)        { return session_error(e); });
				}

				if (session.phase == Session_state::SERVICE_DENIED) {
					_revert_quota_and_destroy(session);
					return Session_error::DENIED;
				}

				if (session.phase == Session_state::INSUFFICIENT_RAM_QUOTA) {
					_revert_quota_and_destroy(session);
					return Session_error::INSUFFICIENT_RAM_QUOTA;
				}

				if (session.phase == Session_state::INSUFFICIENT_CAP_QUOTA) {
					_revert_quota_and_destroy(session);
					return Session_error::INSUFFICIENT_CAP_QUOTA;
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
		);
	};

	Session_result result = Session_error::DENIED;
	with_pd(
		[&] (Pd_session &pd) {
			_policy.with_route(name.string(), label, session_diag_from_args(argbuf),
				[&] (Child_policy::Route const &route) { result = create(pd, route); },
				[&] { });
		},
		[&] { error(_policy.name(), ": PD uninitialized at sesssion-creation time"); });

	return result;
}


Parent::Session_cap_result Child::session_cap(Client::Id id)
{
	return _id_space.apply<Session_state>(id,

		[&] (Session_state &session) -> Session_cap_result {

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
				case Session_state::SERVICE_DENIED:         return Session_cap_error::DENIED;
				case Session_state::INSUFFICIENT_RAM_QUOTA: return Session_cap_error::INSUFFICIENT_RAM_QUOTA;
				case Session_state::INSUFFICIENT_CAP_QUOTA: return Session_cap_error::INSUFFICIENT_CAP_QUOTA;
				default: break;
				}
			}

			if (!session.alive())
				warning(_policy.name(), ": attempt to request cap for unavailable session: ", session);

			if (session.cap.valid())
				session.phase = Session_state::CAP_HANDED_OUT;

			_policy.session_state_changed();

			return session.cap;
		},
		[&] /* missing */ {
			warning(_policy.name(), " requested session cap for unknown ID");
			return Session_capability();
		}
	);
}


Parent::Upgrade_result Child::upgrade(Client::Id id, Parent::Upgrade_args const &args)
{
	if (!args.valid_string()) {
		warning("no valid session-upgrade arguments");
		return Upgrade_result::OK;
	}

	/* ignore suprious request that may arrive after 'close_all_sessions' */
	if (_pd.closed())
		return Upgrade_result::PENDING;

	Upgrade_result result = Upgrade_result::PENDING;

	bool session_state_changed = false;

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
			Ram_transfer::Remote_account ref_ram_account { _policy.ref_account(), _policy.ref_account_cap() };
			Cap_transfer::Remote_account ref_cap_account { _policy.ref_account(), _policy.ref_account_cap() };

			with_pd([&] (Pd_session &pd) {

				Ram_transfer::Remote_account ram_account { pd, pd_session_cap() };
				Cap_transfer::Remote_account cap_account { pd, pd_session_cap() };

				/* transfer quota from client to ourself */
				Ram_transfer ram_donation_from_child(ram_quota, ram_account, ref_ram_account);
				Cap_transfer cap_donation_from_child(cap_quota, cap_account, ref_cap_account);

				/* transfer session quota from ourself to the service provider */
				Ram_transfer ram_donation_to_service(ram_quota, ref_ram_account, session.service());
				Cap_transfer cap_donation_to_service(cap_quota, ref_cap_account, session.service());

				session.increase_donated_quota(ram_quota, cap_quota);
				session.phase = Session_state::UPGRADE_REQUESTED;

				auto upgrade_error = [] (Service::Initiate_error e)
				{
					using Error = Service::Initiate_error;
					switch (e) {
					case Error::OUT_OF_RAM:  return Upgrade_result::OUT_OF_RAM;
					case Error::OUT_OF_CAPS: break;
					}
					return Upgrade_result::OUT_OF_CAPS;
				};

				Service::Initiate_result const initiate_result =
					session.service().initiate_request(session);

				if (initiate_result.failed()) {
					initiate_result.with_error([&] (Service::Initiate_error e) {
						result = upgrade_error(e); });
					return;
				}

				session_state_changed = true;

				/* finish transaction */
				ram_donation_from_child.acknowledge();
				cap_donation_from_child.acknowledge();
				ram_donation_to_service.acknowledge();
				cap_donation_to_service.acknowledge();
			},
			[&] { warning(_policy.name(), ": PD unexpectedly not initialized"); });
		}
		catch (Ram_transfer::Quota_exceeded) {
			warning(_policy.name(), ": RAM upgrade of ", session.service().name(), " failed");
			result = Upgrade_result::OUT_OF_RAM;
			return;
		}
		catch (Cap_transfer::Quota_exceeded) {
			warning(_policy.name(), ": cap upgrade of ", session.service().name(), " failed");
			result = Upgrade_result::OUT_OF_CAPS;
			return;
		}
		if (session.phase == Session_state::CAP_HANDED_OUT) {
			result = Upgrade_result::OK;
			_policy.session_state_changed();
			return;
		}
		session.service().wakeup();
	};

	_id_space.apply<Session_state>(id, upgrade_session, [&] /* missing */ { });

	if (session_state_changed)
		_policy.session_state_changed();

	return result;
}


void Child::_revert_quota_and_destroy(Pd_session &pd, Session_state &session)
{
	Ram_transfer::Remote_account   ref_ram_account(_policy.ref_account(), _policy.ref_account_cap());
	Ram_transfer::Account     &service_ram_account = session.service();
	Ram_transfer::Remote_account child_ram_account(pd, pd_session_cap());

	Cap_transfer::Remote_account   ref_cap_account(_policy.ref_account(), _policy.ref_account_cap());
	Cap_transfer::Account     &service_cap_account = session.service();
	Cap_transfer::Remote_account child_cap_account(pd, pd_session_cap());

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


void Child::_revert_quota_and_destroy(Session_state &session)
{
	with_pd([&] (Pd_session &pd) { _revert_quota_and_destroy(pd, session); },
	        [&] { warning(_policy.name(), ": PD invalid at destruction time"); });
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
		return Close_result::DONE;
	}

	/* close session if alive */
	if (session.alive()) {
		session.phase = Session_state::CLOSE_REQUESTED;
		if (session.service().initiate_request(session).failed())
			warning("failed to initiate close request: ", session);
	}

	/*
	 * The service may have completed the close request immediately (e.g.,
	 * a locally implemented service). In this case, we can skip the
	 * asynchonous handling.
	 */

	if (session.phase == Session_state::CLOSED) {
		_revert_quota_and_destroy(session);
		return Close_result::DONE;
	}

	_policy.session_state_changed();

	session.service().wakeup();

	return Close_result::PENDING;
}


Child::Close_result Child::close(Client::Id id)
{
	/* refuse to close the child's initial sessions */
	if (Parent::Env::session_id(id))
		return Close_result::DONE;

	return _id_space.apply<Session_state>(id,
		[&] (Session_state &session) -> Close_result { return _close(session); },
		[&] /* missing */                            { return Close_result::DONE; });
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
				Ram_transfer::Remote_account ref_ram_account(_policy.ref_account(), _policy.ref_account_cap());
				Ram_transfer::Account   &service_ram_account = session.service();

				Cap_transfer::Remote_account ref_cap_account(_policy.ref_account(), _policy.ref_account_cap());
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
	},
	[&] /* missing ID */ {
		warning("unexpected session response for unknown session");
	});
}


void Child::deliver_session_cap(Server::Id id, Session_capability cap)
{
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
			if (session.service().initiate_request(session).failed())
				warning("failed to initiate close for vanished client: ", session);
			session.service().wakeup();
			return;
		}

		session.cap   = cap;
		session.phase = Session_state::AVAILABLE;

		if (session.ready_callback)
			session.ready_callback->session_ready(session);
	},
	[&] /* missing ID */ { });
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

	if (_start_result == Start_result::OK || _start_result == Start_result::INVALID)
		return;

	with_cpu(
		[&] (Cpu_session &cpu) {
			_policy.init(cpu, _cpu.cap());
			_initial_thread.construct(cpu, _pd.cap(), _policy.name());
		},
		[&] { _error("CPU session missing for initialization"); }
	);

	with_pd(
		[&] (Pd_session &pd) {
			pd.assign_parent(cap());

			if (_policy.forked()) {
				_start_result = Start_result::OK;
			} else {
				_policy.with_address_space(pd, [&] (Genode::Region_map &address_space) {
					_start_result = _start_process(_linker_dataspace(), pd,
					                               *_initial_thread, _initial_thread_start,
					                               _local_rm, address_space, cap());
				});
			}
		},
		[&] { _error("PD session missing for initialization"); }
	);

	if (_start_result == Start_result::OUT_OF_RAM)  _error("out of RAM during ELF loading");
	if (_start_result == Start_result::OUT_OF_CAPS) _error("out of caps during ELF loading");
	if (_start_result == Start_result::INVALID)     _error("attempt to load an invalid executable");
}


void Child::_discard_env_session(Id_space<Parent::Client>::Id id)
{
	_id_space.apply<Session_state>(id,
		[&] (Session_state &s) { s.discard_id_at_client(); },
		[&] /* missing */ { });
}


void Child::initiate_env_pd_session()
{
	_pd.initiate();

	with_pd([&] (Pd_session &pd) { _policy.init(pd, _pd.cap()); }, [&] { });
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
		if (close_result != Close_result::DONE)
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
	auto lambda = [&] (Session_state &s) { _revert_quota_and_destroy(s); };
	while (_policy.server_id_space().apply_any<Session_state>(lambda));

	/*
	 * Issue close requests to the providers of the environment sessions,
	 * which may be async services.
	 */
	_log.close();
	_binary.close();
	if (_linker.constructed())
		_linker->close();

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

		if (close_result == Close_result::PENDING)
			session.discard_id_at_client();
	};

	while (_id_space.apply_any<Session_state>(close_fn));

	_pd.close();

	if (!KERNEL_SUPPORTS_EAGER_CHILD_DESTRUCTION)
		_cpu._connection.destruct();
}


Child::Child(Local_rm        &local_rm,
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
}

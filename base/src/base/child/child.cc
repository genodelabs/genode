/*
 * \brief  Child creation framework
 * \author Norman Feske
 * \date   2006-07-22
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

				/**
				 * Constructor
				 *
				 * \param quantim  number of bytes to transfer
				 * \param from     donator RAM session
				 * \param to       receiver RAM session
				 */
				Transfer(size_t quantum,
				         Ram_session_capability from,
				         Ram_session_capability to)
				: _ack(false), _quantum(quantum), _from(from), _to(to)
				{
					if (_from.valid() && _to.valid() &&
					    Ram_session_client(_from).transfer_quota(_to, quantum)) {
						PWRN("not enough quota for a donation of %zd bytes", quantum);
						throw Parent::Quota_exceeded();
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


/********************
 ** Child::Session **
 ********************/

class Child::Session : public Object_pool<Session>::Entry,
                       public List<Session>::Element
{
	private:

		enum { IDENT_LEN = 16 };

		/**
		 * Session capability at the server
		 */
		Session_capability _cap;

		/**
		 * Service interface that was used to create the session
		 */
		Service *_service;

		/**
		 * Server implementing the session
		 *
		 * Even though we can normally determine the server of the session via
		 * '_service->server()', this does not apply when destructing a server.
		 * During destruction, we use the 'Server' pointer as opaque key for
		 * revoking active sessions of the server. So we keep a copy
		 * independent of the 'Service' object.
		 */
		Server *_server;

		/**
		 * Total of quota associated with this session
		 */
		size_t _donated_ram_quota;

		/**
		 * Name of session, used for debugging
		 */
		char _ident[IDENT_LEN];

	public:

		/**
		 * Constructor
		 *
		 * \param session    session capability
		 * \param service    service that implements the session
		 * \param ram_quota  initial quota donation associated with
		 *                   the session
		 * \param ident      optional session identifier, used for
		 *                   debugging
		 */
		Session(Session_capability session, Service *service,
		        size_t ram_quota, const char *ident = "<noname>")
		:
			Object_pool<Session>::Entry(session), _cap(session),
			_service(service), _server(service->server()),
			_donated_ram_quota(ram_quota) {
			strncpy(_ident, ident, sizeof(_ident)); }

		/**
		 * Default constructor creates invalid session
		 */
		Session() : _service(0), _donated_ram_quota(0) { }

		/**
		 * Extend amount of ram attached to the session
		 */
		void upgrade_ram_quota(size_t ram_quota) {
			_donated_ram_quota += ram_quota; }

		/**
		 * Accessors
		 */
		Session_capability cap()               const { return _cap; }
		size_t             donated_ram_quota() const { return _donated_ram_quota; }
		bool               valid()             const { return _service != 0; }
		Service           *service()           const { return _service; }
		Server            *server()            const { return _server; }
		const char        *ident()             const { return _ident; }
};


/***********
 ** Child **
 ***********/

void Child::_add_session(Child::Session const &s)
{
	Lock::Guard lock_guard(_lock);

	/*
	 * Store session information in a new child's meta data structure.  The
	 * allocation from 'heap()' may throw a 'Ram_session::Quota_exceeded'
	 * exception.
	 */
	Session *session = 0;
	try {
		session = new (heap())
		          Session(s.cap(), s.service(),
		                  s.donated_ram_quota(), s.ident()); }
	catch (Allocator::Out_of_memory) {
		throw Parent::Quota_exceeded(); }

	/* these functions may also throw 'Ram_session::Quota_exceeded' */
	_session_pool.insert(session);
	_session_list.insert(session);
}


void Child::_remove_session(Child::Session *s)
{
	/* forget about this session */
	_session_pool.remove(s);
	_session_list.remove(s);

	/* return session quota to the ram session of the child */
	if (_policy->ref_ram_session()->transfer_quota(_ram, s->donated_ram_quota()))
		PERR("We ran out of our own quota");

	destroy(heap(), s);
}


Service *Child::_parent_service()
{
	static Parent_service parent_service("");
	return &parent_service;
}


void Child::revoke_server(Server const *server)
{
	Lock::Guard lock_guard(_lock);

	while (1) {
		/* search session belonging to the specified server */
		Session *s = _session_list.first();
		for ( ; s && (s->server() != server); s = s->next());

		/* if no matching session exists, we are done */
		if (!s) return;

		_remove_session(s);
	}
}


void Child::announce(Parent::Service_name const &name, Root_capability root)
{
	if (!name.is_valid_string()) return;

	_policy->announce_service(name.string(), root, heap(), &_server);
}


Session_capability Child::session(Parent::Service_name const &name,
                                  Parent::Session_args const &args)
{
	if (!name.is_valid_string() || !args.is_valid_string()) throw Unavailable();

	/* return sessions that we created for the child */
	if (!strcmp("Env::ram_session", name.string())) return _ram;
	if (!strcmp("Env::cpu_session", name.string())) return _cpu;
	if (!strcmp("Env::rm_session",  name.string())) return _rm;
	if (!strcmp("Env::pd_session",  name.string())) return _process.pd_session_cap();

	/* filter session arguments according to the child policy */
	strncpy(_args, args.string(), sizeof(_args));
	_policy->filter_session_args(name.string(), _args, sizeof(_args));

	/* transfer the quota donation from the child's account to ourself */
	size_t ram_quota = Arg_string::find_arg(_args, "ram_quota").long_value(0);

	Transfer donation_from_child(ram_quota, _ram, env()->ram_session_cap());

	Service *service = _policy->resolve_session_request(name.string(), _args);

	/* raise an error if no matching service provider could be found */
	if (!service)
		throw Service_denied();

	/* transfer session quota from ourself to the service provider */
	Transfer donation_to_service(ram_quota, env()->ram_session_cap(),
	                             service->ram_session_cap());

	/* create session */
	Session_capability cap;
	try { cap = service->session(_args); }
	catch (Service::Invalid_args)   { throw Service_denied(); }
	catch (Service::Unavailable)    { throw Service_denied(); }
	catch (Service::Quota_exceeded) { throw Quota_exceeded(); }

	/* register session */
	try { _add_session(Session(cap, service, ram_quota, name.string())); }
	catch (Ram_session::Quota_exceeded) { throw Quota_exceeded(); }

	/* finish transaction */
	donation_from_child.acknowledge();
	donation_to_service.acknowledge();

	return cap;
}


void Child::upgrade(Session_capability to_session, Parent::Upgrade_args const &args)
{
	Service *targeted_service = 0;

	/* check of upgrade refers to an Env:: resource */
	if (to_session.local_name() == _ram.local_name())
		targeted_service = &_ram_service;
	if (to_session.local_name() == _cpu.local_name())
		targeted_service = &_cpu_service;
	if (to_session.local_name() == _rm.local_name())
		targeted_service = &_rm_service;

	/* check if upgrade refers to server */
	Session * const session = _session_pool.obj_by_cap(to_session);
	if (session)
		targeted_service = session->service();

	if (!targeted_service) {
		PWRN("could not lookup service for session upgrade");
		return;
	}

	if (!args.is_valid_string()) {
		PWRN("no valid session-upgrade arguments");
		return;
	}

	size_t const ram_quota =
		Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);

	/* transfer quota from client to ourself */
	Transfer donation_from_child(ram_quota, _ram,
	                             env()->ram_session_cap());

	/* transfer session quota from ourself to the service provider */
	Transfer donation_to_service(ram_quota, env()->ram_session_cap(),
	                             targeted_service->ram_session_cap());

	try { targeted_service->upgrade(to_session, args.string()); }
	catch (Service::Quota_exceeded) { throw Quota_exceeded(); }

	/* remember new amount attached to the session */
	if (session)
		session->upgrade_ram_quota(ram_quota);

	/* finish transaction */
	donation_from_child.acknowledge();
	donation_to_service.acknowledge();
}


void Child::close(Session_capability session_cap)
{
	/* refuse to close the child's initial sessions */
	if (session_cap.local_name() == _ram.local_name()
	 || session_cap.local_name() == _cpu.local_name()
	 || session_cap.local_name() == _rm.local_name()
	 || session_cap.local_name() == _process.pd_session_cap().local_name())
		return;

	Session *s = _session_pool.obj_by_cap(session_cap);

	if (!s) {
		PWRN("no session structure found");
		return;
	}

	/*
	 * There is a chance that the server is not responding to the 'close' call,
	 * making us block infinitely. However, by using core's cancel-blocking
	 * mechanism, we can cancel the 'close' call by another (watchdog) thread
	 * that invokes 'cancel_blocking' at our thread after a timeout. The
	 * unblocking is reflected at the API level as an 'Blocking_canceled'
	 * exception. We catch this exception to proceed with normal operation
	 * after being unblocked.
	 */
	try { s->service()->close(s->cap()); }
	catch (Blocking_canceled) {
		PDBG("Got Blocking_canceled exception during %s->close call\n",
		     s->ident()); }

	/*
	 * If the session was provided by a child of us,
	 * 'server()->ram_session_cap()' returns the RAM session of the
	 * corresponding child. Since the session to the server is closed now, we
	 * expect that the server released all donated resources and we can
	 * decrease the servers' quota.
	 *
	 * If this goes wrong, the server is misbehaving.
	 */
	if (s->service()->ram_session_cap().valid()) {
		Ram_session_client server_ram(s->service()->ram_session_cap());
		if (server_ram.transfer_quota(env()->ram_session_cap(),
		                              s->donated_ram_quota())) {
			PERR("Misbehaving server '%s'!", s->service()->name());
		}
	}

	Lock::Guard lock_guard(_lock);
	_remove_session(s);
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
	return _policy->exit(exit_value);
}


Child::Child(Dataspace_capability    elf_ds,
             Ram_session_capability  ram,
             Cpu_session_capability  cpu,
             Rm_session_capability   rm,
             Rpc_entrypoint         *entrypoint,
             Child_policy           *policy,
             Service                &ram_service,
             Service                &cpu_service,
             Service                &rm_service)
:
	_ram(ram), _ram_session_client(ram), _cpu(cpu), _rm(rm),
	_ram_service(ram_service), _cpu_service(cpu_service),
	_rm_service(rm_service),
	_heap(&_ram_session_client, env()->rm_session()),
	_entrypoint(entrypoint),
	_parent_cap(_entrypoint->manage(this)),
	_policy(policy),
	_server(ram),
	_process(elf_ds, ram, cpu, rm, _parent_cap, policy->name(), 0)
{ }


Child::~Child()
{
	_entrypoint->dissolve(this);
	_policy->unregister_services();

	for (Session *s; (s = _session_pool.first()); )
		close(s->cap());
}


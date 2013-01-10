/*
 * \brief  Declaration of a round robin scheduler
 * \author Martin stein
 * \date   2010-06-25
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__GENERIC__INCLUDE__SCHEDULER_H_
#define _KERNEL__GENERIC__INCLUDE__SCHEDULER_H_

/* generic includes */
#include <generic/timer.h>
#include <generic/verbose.h>

/* util includes */
#include <util/queue.h>

namespace Kernel {

	enum { SHOW_SCHEDULING = 0 };

	enum { SCHEDULER__TRACE   = 1,
	       SCHEDULER__VERBOSE = 0,
	       SCHEDULER__ERROR   = 1,
	       SCHEDULER__WARNING = 1 };

	class Exec_context;
	class Platform;

	class Scheduler : public Tracks_time
	{
		enum { MS_PER_ROUND_PER_CLIENT    = SCHEDULING_MS_INTERVAL,
		       SCHEDULE__VERBOSE__SUCCESS = SCHEDULER__VERBOSE,
		       SCHEDULE__ERROR            = SCHEDULER__ERROR };

		public:

			typedef Scheduling_timer Timer;
			typedef unsigned int     Quota;
			typedef Kernel::Platform Ressource;

		private:

			Timer * const _timer;
			const Quota  _quota_per_round_per_client;
			Ressource*   _ressource;
			bool         _new_clients;

			void _schedule();

			inline Quota _ms_to_quota(unsigned int const &ms);

			/**
			 * Utilise idle client as current client
			 */
			inline void _prep_idle_round();

		public:

			inline void time_consumed(Quota const & q);

			enum { CLIENT__WARNING = 1,
			       CLIENT__VERBOSE = SCHEDULER__VERBOSE };

			class Client_queue;

			class Client : public Kernel::Queue<Client>::Item
			{
				friend class Scheduler;
				friend class Client_queue;

				typedef Kernel::Scheduler::Quota Quota;

				Quota      _quota;
				Scheduler *_scheduler;
				bool       _sleeping;

				protected:

					typedef Kernel::Exec_context Context;

				private:

					inline Quota    _consume(Quota const &consumed);
					inline void     _earn_quota(Quota const &q);
					inline Context *_schedulable_context();

				protected:

					enum{ SCHEDULABLE_context__VERBOSE = 1 };

					inline Client();

					virtual ~Client();

					inline void _sleep();
					inline void _wake();

					virtual Context *_context() = 0;
					virtual bool     _preemptable() = 0;

				public:

					virtual int label() = 0;
			};


			struct Client_queue : public Kernel::Queue<Client>
			{
				inline void print_state();
			};

		private:

			Client_queue _client_queue;
			Client*      _current_client;
			Client*      _last_client;
			Client*      _idle_client;

		public:

			enum{ ADD__VERBOSE    = SCHEDULER__VERBOSE||SHOW_SCHEDULING,
			      REMOVE__VERBOSE = SCHEDULER__VERBOSE||SHOW_SCHEDULING,
			      RUN__VERBOSE    = SCHEDULER__VERBOSE||SHOW_SCHEDULING };

			/**
			 * Constructor
			 * \param  r  Ressource that is shared by the clients
			 * \param  t  Timer to measure exclusive access duration
			 * \param  idle_client  this client gets scheduled if there's
			 *                      no other client, it can't be removed
			 */
			Scheduler(Ressource* r, Scheduling_timer * const t, Client* idle_client);

			void add(Client* c);
			void remove(Client* c);
			void run();

			inline Client* current_client();

			inline void skip_next_time(Client* c);

		protected:

			/* debugging */
			inline void _print_clients_via_labels();
			inline void _run__verbose__success();
			inline void _run__error__no_ready_client();
			inline void _run__trace__client_checks();
			inline void _schedule__error__no_clients();
			inline void _schedule__verbose__success() ;
			inline void _remove__warning__invalid_client();
			inline void _remove__verbose__success(Client* c);
			inline void _remove__trace(Client *c);
			inline void _add__warning__invalid_client();
			inline void _add__verbose__success();
	};

	/**
	 * Pointer to kernels static scheduler for execution time
	 */
	Scheduler *scheduler();
}


/***********************************
 ** Kernel::Scheduler definitions **
 ***********************************/


void Kernel::Scheduler::_prep_idle_round()
{
	if(_current_client) { _client_queue.enqueue(_current_client); }
	_current_client=_idle_client;
}


void Kernel::Scheduler::time_consumed(Quota const & q)
{
	_current_client->_consume(q);
}


Kernel::Scheduler::Client* Kernel::Scheduler::current_client()
{
	return _current_client;
}


void Kernel::Scheduler::skip_next_time(Client *c) { c->_quota=0; }


Kernel::Scheduler::Quota Kernel::Scheduler::_ms_to_quota(unsigned int const & ms)
{
	return _timer->msec_to_native(ms);
}


void Kernel::Scheduler::_print_clients_via_labels()
{
	printf("scheduled ");
	_last_client ? printf("%i", _last_client->label())
	             : printf("ø");
	printf("→");
	_current_client ? printf("%i", _current_client->label())
	                : printf("ø");
	printf(", queue ");
	_client_queue.print_state();
}


void Kernel::Scheduler::_run__verbose__success()
{
	if (!RUN__VERBOSE)
		return;

	printf("Kernel::Scheduler::run, ");
	_print_clients_via_labels();
	printf("\n");
}


void Kernel::Scheduler::_run__error__no_ready_client()
{
	if (!SCHEDULER__ERROR) return;

	printf("Error in Kernel::Scheduler::run, no client is ready, halt\n");
	halt();
}


void Kernel::Scheduler::_remove__trace(Client* c)
{
	if (SCHEDULER__TRACE && Verbose::trace_current_kernel_pass())
		printf("rm(%i) ", c->label());
}


void Kernel::Scheduler::_run__trace__client_checks()
{
	if (SCHEDULER__TRACE && Verbose::trace_current_kernel_pass())
		printf("ask(%i,%i) ",
		       _current_client->label(), _current_client->_quota);
}


void Kernel::Scheduler::_schedule__error__no_clients()
{
	if (!SCHEDULER__ERROR) return;

	printf("Error in Kernel::Scheduler::_schedule, no clients registered, halt\n");
	halt();
}


void Kernel::Scheduler::_remove__warning__invalid_client()
{
	if (!SCHEDULER__WARNING) return;

	printf("Warning in Kernel::Scheduler::remove, client invalid, skip\n");
}


void Kernel::Scheduler::_add__warning__invalid_client()
{
	if (!SCHEDULER__WARNING) return;

	printf("Warning in Kernel::Scheduler::add, client invalid, skip\n");
}


void Kernel::Scheduler::_add__verbose__success()
{
	if (!ADD__VERBOSE) return;

	printf("Kernel::Scheduler::add, ");
	_print_clients_via_labels();
	printf(" ← )\n");
}


void Kernel::Scheduler::_remove__verbose__success(Client* c)
{
	if (!REMOVE__VERBOSE) return;

	printf("Kernel::Scheduler::remove, ");
	_print_clients_via_labels();
	printf(" → %i\n", c->label());
}


void Kernel::Scheduler::_schedule__verbose__success()
{
	if (!SCHEDULER__VERBOSE) return;

	Client* const a = _last_client;
	Client* const b = _current_client;

	Verbose::indent(10);
	if (a) printf("from %i", a->label());
	else   printf("from NULL");

	Verbose::indent(10);
	printf("to %i\n", b->label());
}


/*******************************************
 ** Kernel::Scheduler::Client definitions **
 *******************************************/

Kernel::Scheduler::Client::Context *
Kernel::Scheduler::Client::_schedulable_context()
{
	Context *result = 0;
	if (!_sleeping) {
		result = _context();
		if (_sleeping)
			result = 0;
	}
	return result;
}


Kernel::Scheduler::Client::Quota
Kernel::Scheduler::Client::_consume(Quota const &consumed)
{
	if (consumed > _quota) {
		_quota = 0;
	} else{
		_quota = _quota - consumed;
	}
	return _quota;
}


Kernel::Scheduler::Client::Client()
: _quota(0), _scheduler(0), _sleeping(false) { }


void Kernel::Scheduler::Client::_earn_quota(Quota const &q) { _quota += q; }


void Kernel::Scheduler::Client::_sleep() { _sleeping = true; }


void Kernel::Scheduler::Client::_wake(){ _sleeping = false; }


/*************************************************
 ** Kernel::Scheduler::Client_queue definitions **
 *************************************************/

void Kernel::Scheduler::Client_queue::print_state()
{
	Client *i = _head;
	if (!i) printf("ø");
	while (i) {
		printf("%i", i->label());
		if (i != _tail) printf("→");
		i = i->_next;
	}
}

#endif /* _KERNEL__INCLUDE__SCHEDULER_H_ */


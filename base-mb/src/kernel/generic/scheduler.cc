/*
 * \brief  Implementation of a round robin scheduler
 * \author Martin stein
 * \date   2010-06-22
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Generic includes */
#include <generic/scheduler.h>
#include <kernel/types.h>
#include <kernel/config.h>
#include "generic/verbose.h"

/* Platform includes */
#include <platform/platform.h>

using namespace Kernel;

extern unsigned int _current_context_label;


Scheduler::Scheduler(Ressource * const r, Scheduling_timer * const t, 
                     Client * const idle_client)
:
	_timer(t),
	_quota_per_round_per_client(_ms_to_quota(MS_PER_ROUND_PER_CLIENT)),
	_ressource(r),
	_current_client(0),
	_idle_client(idle_client)
{
	_idle_client->_scheduler=this;
}


void Scheduler::_schedule()
{
	_last_client=_current_client;
	if (_last_client && _last_client != _idle_client) {
		_client_queue.enqueue(_last_client);
	}

	_current_client=_client_queue.dequeue();
	if (!_current_client){
		_current_client=_idle_client;
	}
}


Kernel::Scheduler::Client::~Client()
{
	if (_scheduler)
		_scheduler->remove(this);
}


void Scheduler::run()
{
	if (!_current_client){
		_schedule();
		if (!_current_client)
			_run__error__no_ready_client();
	}

	_new_clients = false;
	Client* first_client = _current_client;
	Client::Context *c = 0;

	while (1) {

		_run__trace__client_checks();
		c = _current_client->_schedulable_context();

		if (c && _current_client) {
			if (_current_client->_quota) {
				break;
			} else {
				_current_client->_earn_quota(_quota_per_round_per_client);
				_new_clients = true;
			}
		}
		_schedule();

		if (_new_clients) {
			first_client = _current_client;
			_new_clients = false;
		}
		else if (_current_client == first_client){
			_prep_idle_round();
		}
	}

	_current_context_label = (unsigned int)_current_client->label();
	_timer->track_time(_current_client->_quota, this);
	_ressource->lock(c);
	_run__verbose__success();
}


void Scheduler::add(Client *c)
{
	if (!c) { return; }
	if (c == _idle_client) { return; }
	if (c->_scheduler == this) { return; }
	if (c->_scheduler) { c->_scheduler->remove(c); }

	c->_quota = _quota_per_round_per_client;
	c->_scheduler = this;
	_client_queue.enqueue(c);
	_new_clients = true;
}


void Scheduler::remove(Client* c)
{
	if (!c) { return; }
	if (c->_scheduler != this) { return; }
	if (c == _idle_client) { return; }

	if (_current_client == c) { _current_client = 0; }
	else _client_queue.remove(c);
	c->_scheduler = 0;
	_new_clients = true;
}


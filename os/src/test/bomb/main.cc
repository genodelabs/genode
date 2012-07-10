/*
 * \brief  Fork bomb to stress Genode
 * \author Christian Helmuth
 * \date   2007-08-16
 *
 * The better part of this code is derived from the original init
 * implementation by Norman.
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/child.h>
#include <base/sleep.h>
#include <base/service.h>
#include <base/snprintf.h>
#include <init/child_policy.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>
#include <rom_session/connection.h>
#include <cap_session/connection.h>
#include <rm_session/connection.h>
#include <timer_session/connection.h>

using namespace Genode;


class Bomb_child_resources
{
	protected:

		Genode::Rom_connection _rom;
		Genode::Ram_connection _ram;
		Genode::Cpu_connection _cpu;
		Genode::Rm_connection  _rm;
		char                   _name[32];

		Bomb_child_resources(const char *file_name, const char *name,
		                     Genode::size_t ram_quota)
		:
			_rom(file_name, name), _ram(name), _cpu(name)
		{
			Genode::strncpy(_name, name, sizeof(_name));

			_ram.ref_account(env()->ram_session_cap());
			Genode::env()->ram_session()->transfer_quota(_ram.cap(), ram_quota);

			if (!_ram.cap().valid() || !_cpu.cap().valid()) {
				class Ram_or_cpu_session_not_valid { };
				throw Ram_or_cpu_session_not_valid();
			}
		}
};


class Bomb_child : private Bomb_child_resources,
                   public  Genode::Child_policy,
                   private Init::Child_policy_enforce_labeling,
                   public  Genode::List<Bomb_child>::Element
{
	private:

		/*
		 * Entry point used for serving the parent interface
		 */
		enum { STACK_SIZE = 8*1024 };
		Genode::Rpc_entrypoint _entrypoint;

		Genode::Child             _child;
		Genode::Service_registry *_parent_services;

	public:

		Bomb_child(const char       *file_name,
		           const char       *unique_name,
		           Genode::size_t    ram_quota,
		           Cap_session      *cap_session,
		           Service_registry *parent_services)
		:
			Bomb_child_resources(file_name, unique_name, ram_quota),
			Init::Child_policy_enforce_labeling(Bomb_child_resources::_name),
			_entrypoint(cap_session, STACK_SIZE, "bomb", false),
			_child(_rom.dataspace(), _ram.cap(), _cpu.cap(), _rm.cap(),
			       &_entrypoint, this),
			_parent_services(parent_services) {
			_entrypoint.activate(); }

		~Bomb_child() { PDBG("called"); }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return Bomb_child_resources::_name; }

		void filter_session_args(const char *, char *args, Genode::size_t args_len)
		{
			Child_policy_enforce_labeling::filter_session_args(0, args, args_len);
		}

		Service *resolve_session_request(const char *service_name,
		                                 const char *args)
		{
			return _parent_services->find(service_name);
		}
};


/*
 * List of children
 *
 * Access to the children list from different threads
 * must be synchronized via the children lock.
 */
static Lock             _children_lock;
static List<Bomb_child> _children;


/**
 * Check if a program with the specified name already exists
 */
static bool child_name_exists(const char *name)
{
	Bomb_child *c = _children.first();

	for ( ; c; c = c->List<Bomb_child>::Element::next())
		if (strcmp(c->name(), name) == 0)
			return true;

	return false;
}


/**
 * Create a unique name based on the filename
 *
 * If a program with the filename as name already exists, we
 * add a counting number as suffix.
 */
static void get_unique_child_name(const char *filename, char *dst, size_t dst_len)
{
	Lock::Guard lock_guard(_children_lock);

	char buf[32];
	char suffix[8];
	suffix[0] = 0;

	for (int cnt = 1; true; cnt++) {

		/* build program name composed of filename and numeric suffix */
		snprintf(buf, sizeof(buf), "%s%s", filename, suffix);

		/* if such a program name does not exist yet, we are happy */
		if (!child_name_exists(buf)) {
			strncpy(dst, buf, dst_len);
			return;
		}

		/* increase number of suffix */
		snprintf(suffix, sizeof(suffix), ".%d", cnt + 1);
	}
}


/**
 * Start a child
 */
static int start_child(const char *file_name, Cap_session *cap_session,
                       size_t ram_quota, Service_registry *parent_services)
{
	char name[64];
	get_unique_child_name(file_name, name, sizeof(name));

	Bomb_child *c = new (env()->heap())
		Bomb_child(file_name, name, ram_quota, cap_session, parent_services);

	Lock::Guard lock_guard(_children_lock);
	_children.insert(c);
	return 0;
}


/**
 * Kill child
 */
static void exit_child(Bomb_child *child)
{
	destroy(env()->heap(), child);
}


/**
 * Request timer service
 *
 * \return  timer session, or 0 if bomb is our parent
 */
Timer::Session *timer()
{
	try {
		static Timer::Connection timer_inst;
		return &timer_inst;
	} catch (Parent::Service_denied) { }

	return 0;
}


int main(int argc, char **argv)
{
	printf("--- bomb started ---\n");

	/* connect to core's cap service used for creating parent capabilities */
	Cap_connection cap;

	/* names of services provided by the parent */
	static const char *names[] = {
		"CAP", "RAM", "RM", "PD", "CPU", "ROM", "LOG", 0 };

	static Service_registry parent_services;
	for (unsigned i = 0; names[i]; i++)
		parent_services.insert(new (env()->heap()) Parent_service(names[i]));

	const long children = 2;
	const long demand   = 1024 * 1024;

	unsigned long avail = env()->ram_session()->avail();
	long amount = (avail - demand) / children;
	if (amount < (children * demand)) {
		PDBG("I'm a leaf node.");
		sleep_forever();
	}

	while (1) {
		for (unsigned i = children; i; --i)
			start_child("bomb", &cap, amount, &parent_services);

		/* is init our parent? */
		if (!timer()) sleep_forever();

		timer()->msleep(2000);
		PDBG("It's time to kill all my children...");

		while (1) {
			Bomb_child *c;

			_children_lock.lock();
			c = _children.first();
			if (c) _children.remove(c);
			_children_lock.unlock();

			if (c) exit_child(c);
			else break;
		}

		PDBG("Done.");
	}

	sleep_forever();
	return 0;
}

/*
 * \brief  Fork bomb to stress Genode
 * \author Christian Helmuth
 * \date   2007-08-16
 *
 * The better part of this code is derived from the original init
 * implementation by Norman.
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
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
#include <pd_session/connection.h>
#include <timer_session/connection.h>

#include <os/config.h>
#include <os/child_policy_dynamic_rom.h>
#include <util/xml_node.h>

using namespace Genode;


class Bomb_child_resources
{
	protected:

		Genode::Session_label _rom_label;

		Genode::Pd_connection  _pd;
		Genode::Rom_connection _rom;
		Genode::Ram_connection _ram;
		Genode::Cpu_connection _cpu;

		typedef String<32> Name;
		Name _name;

		Genode::Region_map_client _address_space { _pd.address_space() };

		Bomb_child_resources(const char *elf_name, const char *name,
		                     Genode::size_t ram_quota)
		:
			_rom_label(Genode::prefixed_label(Genode::Session_label(name),
			                                  Genode::Session_label(elf_name))),
			_pd(name), _rom(_rom_label.string()),
			_ram(name), _cpu(name), _name(name)
		{
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
                   public  Genode::List<Bomb_child>::Element
{
	private:

		Init::Child_policy_enforce_labeling _enforce_labeling_policy;

		Genode::Child::Initial_thread _initial_thread;

		/*
		 * Entry point used for serving the parent interface
		 */
		enum { STACK_SIZE =  2048 * sizeof(Genode::addr_t) };
		Genode::Rpc_entrypoint _entrypoint;

		Genode::Child                          _child;
		Genode::Service_registry              *_parent_services;
		Genode::Child_policy_dynamic_rom_file  _config_policy;

	public:

		Bomb_child(const char       *file_name,
		           const char       *unique_name,
		           Genode::size_t    ram_quota,
		           Cap_session      *cap_session,
		           Service_registry *parent_services,
		           unsigned          generation)
		:
			Bomb_child_resources(file_name, unique_name, ram_quota),
			_enforce_labeling_policy(_name.string()),
			_initial_thread(_cpu, _pd, unique_name),
			_entrypoint(cap_session, STACK_SIZE, "bomb_ep_child", false),
			_child(_rom.dataspace(), Genode::Dataspace_capability(),
			       _pd, _pd, _ram, _ram, _cpu, _initial_thread,
			       *Genode::env()->rm_session(), _address_space, _entrypoint, *this),
			_parent_services(parent_services),
			_config_policy("config", _entrypoint, &_ram)
		{
			char client_config[64];
			snprintf(client_config, sizeof(client_config),
			         "<config generations=\"%u\"/>", generation);
			_config_policy.load(client_config, strlen(client_config) + 1);

			_entrypoint.activate();
		}

		~Bomb_child() { Genode::log(__PRETTY_FUNCTION__); }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return Bomb_child_resources::_name.string(); }

		void filter_session_args(const char * x, char *args, Genode::size_t args_len)
		{
			_enforce_labeling_policy.filter_session_args(0, args, args_len);
		}

		Service *resolve_session_request(const char *service_name,
		                                 const char *args)
		{
			Service * service = nullptr;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(service_name,
			                                                      args)))
				return service;

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
static void get_unique_child_name(const char *filename, char *dst,
                                  size_t dst_len, unsigned generation)
{
	Lock::Guard lock_guard(_children_lock);

	char buf[32];
	char suffix[8];
	suffix[0] = 0;

	for (int cnt = 1; true; cnt++) {

		/* build program name composed of filename and numeric suffix */
		snprintf(buf, sizeof(buf), "%s_g%u%s", filename, generation, suffix);

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
                       size_t ram_quota, Service_registry *parent_services,
                       unsigned generation)
{
	char name[64];
	get_unique_child_name(file_name, name, sizeof(name), generation);

	Bomb_child *c = new (env()->heap())
		Bomb_child(file_name, name, ram_quota, cap_session, parent_services,
		           generation);

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
	Genode::Xml_node node = config()->xml_node();

	unsigned const rounds      = node.attribute_value("rounds", 1U);
	unsigned const generation  = node.attribute_value("generations", 1U);
	unsigned const children    = node.attribute_value("children", 2U);
	unsigned const sleeptime   = node.attribute_value("sleep", 2000U);
	unsigned long const demand = node.attribute_value("demand", 1024UL * 1024);

	log("--- bomb started ---");

	/* connect to core's cap service used for creating parent capabilities */
	Cap_connection cap;

	/* names of services provided by the parent */
	static const char *names[] = {
		"CAP", "RAM", "RM", "PD", "CPU", "ROM", "LOG", 0 };

	static Service_registry parent_services;
	for (unsigned i = 0; names[i]; i++)
		parent_services.insert(new (env()->heap()) Parent_service(names[i]));

	unsigned long avail = env()->ram_session()->avail();
	unsigned long amount = (avail - demand) / children;
	if (amount < (demand * children)) {
		log("I'm a leaf node - generation ", generation, " - not enough memory.");
		sleep_forever();
	}
	if (generation == 0) {
		log("I'm a leaf node - generation 0");
		sleep_forever();
	}

	for (unsigned round = 0; round < rounds ; ++round) {
		for (unsigned i = children; i; --i)
			start_child("bomb", &cap, amount, &parent_services, generation - 1);

		/* is init our parent? */
		if (!timer()) sleep_forever();

		/* don't ask parent for further resources if we ran out of memory */
		static Signal_receiver sig_rec;
		static Signal_context  sig_ctx_res_avail;
		if (round == 0) {
			/* prevent to block for resource upgrades caused by clients */
			env()->parent()->resource_avail_sigh(sig_rec.manage(&sig_ctx_res_avail));
		}

		timer()->msleep(sleeptime);
		log("[", round, "] It's time to kill all my children...");

		while (1) {
			Bomb_child *c;

			_children_lock.lock();
			c = _children.first();
			if (c) _children.remove(c);
			_children_lock.unlock();

			if (c) exit_child(c);
			else break;
		}

		log("[", round, "] Done.");
	}

	/* master if we have a timer connection */
	if (timer())
		log("Done. Going to sleep");

	sleep_forever();
	return 0;
}

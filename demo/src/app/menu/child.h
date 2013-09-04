/*
 * \brief  Process started as a child of the menu
 * \author Norman Feske
 * \date   2010-10-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CHILD_H_
#define _CHILD_H_

/* Genode includes */
#include <base/child.h>
#include <init/child_policy.h>
#include <init/child_config.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>


/***************
 ** Utilities **
 ***************/

/**
 * Read priority-levels declaration from config file
 */
inline long read_prio_levels_log2()
{
	using namespace Genode;

	long prio_levels = 0;
	try {
		config()->xml_node().attribute("prio_levels").value(&prio_levels); }
	catch (...) { }

	if (prio_levels && prio_levels != (1 << log2(prio_levels))) {
		printf("Warning: Priolevels is not power of two, priorities are disabled\n");
		prio_levels = 0;
	}
	return prio_levels ? log2(prio_levels) : 0;
}


inline long read_priority(Genode::Xml_node node)
{
	long priority = Genode::Cpu_session::DEFAULT_PRIORITY;
	try { node.attribute("priority").value(&priority); }
	catch (...) { }

	/*
	 * All priority declarations in the config file are
	 * negative because child priorities can never be higher
	 * than parent priorities. To simplify priority
	 * calculations, we use inverted values. Lower values
	 * correspond to higher priorities.
	 */
	return -priority;
}


inline Genode::size_t read_ram_quota(Genode::Xml_node node)
{
	Genode::Number_of_bytes ram_quota = 0;
	try {
		Genode::Xml_node rsc = node.sub_node("resource");
		for (;; rsc = rsc.next("resource")) {

			try {
				if (rsc.attribute("name").has_value("RAM")) {
					rsc.attribute("quantum").value(&ram_quota);
				}
			} catch (...) { }
		}
	} catch (...) { }

	/*
	 * If the configured quota exceeds our own quota, we donate
	 * all remaining quota to the child but we need to count in
	 * our allocation of the child meta data from the heap.
	 * Hence, we preserve 64K of our own quota.
	 */
	if (ram_quota > Genode::env()->ram_session()->avail() - 64*1024) {
		ram_quota = Genode::env()->ram_session()->avail() - 64*1024;
		Genode::printf("Warning: Specified quota exceeds available quota.\n"
		               "         Proceeding with a quota of %zd bytes.\n",
		               (Genode::size_t)ram_quota);
	}
	return ram_quota;
}


class Menu_child : Genode::Child_policy
{
	private:

		struct Name
		{
			enum { MAX_NAME_LEN = 64 };
			char file[MAX_NAME_LEN];
			char unique[MAX_NAME_LEN];

			Name(Genode::Xml_node node) {
				try {
					node.attribute("name").value(unique, sizeof(unique)); }
				catch (Genode::Xml_node::Nonexistent_attribute) {
					PWRN("Missing 'name' attribute in '<entry>' entry.\n");
					throw; }

					/* use name as default file name if not declared otherwise */
					Genode::strncpy(file, unique, sizeof(file));

					/* check for a binary declaration */
					try {
						Genode::Xml_node binary = node.sub_node("binary");
						binary.attribute("name").value(file, sizeof(file));
					} catch (...) { }
			}
		} _name;

		/**
		 * Resources assigned to the child
		 */
		struct Resources
		{
			long                   prio_levels_log2;
			long                   priority;
			Genode::size_t         ram_quota;
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;
			Genode::Rm_connection  rm;

			Resources(Genode::Xml_node node, const char *label,
			 long prio_levels_log2)
			:
				prio_levels_log2(prio_levels_log2),
				priority(read_priority(node)),
				ram_quota(read_ram_quota(node)),
				ram(label),
				cpu(label, priority*(Genode::Cpu_session::PRIORITY_LIMIT >> prio_levels_log2))
			{
				ram.ref_account(Genode::env()->ram_session_cap());
				Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota);
			}
		} _resources;

		/*
		 * Entry point used for serving the parent interface
		 */
		enum { STACK_SIZE = 8*1024 };
		Genode::Rpc_entrypoint _entrypoint;

		/**
		 * ELF binary
		 */
		Genode::Rom_connection _binary_rom;

		/**
		 * Private child configuration
		 */
		Init::Child_config _config;

		Genode::Child _child;

		Genode::Service_registry *_parent_services;

		Init::Child_policy_enforce_labeling      _labeling_policy;
		Init::Child_policy_handle_cpu_priorities _priority_policy;
		Init::Child_policy_provide_rom_file      _config_policy;
		Init::Child_policy_provide_rom_file      _binary_policy;
		Init::Child_policy_redirect_rom_file     _configfile_policy;

	public:

		Menu_child(Genode::Xml_node          node,
		           Genode::Service_registry *parent_services,
		           long                      prio_levels_log2,
		           Genode::Cap_session      *cap_session)
		:
			_name(node),
			_resources(node, _name.unique, prio_levels_log2),
			_entrypoint(cap_session, STACK_SIZE, "child", false),
			_binary_rom(_name.file, _name.unique),
			_config(_resources.ram.cap(), node),
			_child(_binary_rom.dataspace(), _resources.ram.cap(),
			       _resources.cpu.cap(), _resources.rm.cap(), &_entrypoint, this),
			_parent_services(parent_services),
			_labeling_policy(_name.unique),
			_priority_policy(_resources.prio_levels_log2, _resources.priority),
			_config_policy("config", _config.dataspace(), &_entrypoint),
			_binary_policy("binary", _binary_rom.dataspace(), &_entrypoint),
			_configfile_policy("config", _config.filename())
		{
			_entrypoint.activate();
		}


		/****************************
		 ** Child_policy interface **
		 ****************************/

		const char *name() const { return _name.unique; }

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			Genode::Service *service = 0;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(service_name, args)))
				return service;

			/* check for binary file request */
			if ((service = _binary_policy.resolve_session_request(service_name, args)))
				return service;

			/* populate parent service registry on demand */
			service = _parent_services->find(service_name);
			if (!service) {
				try {
					service = new (Genode::env()->heap())
					Genode::Parent_service(service_name);
					_parent_services->insert(service);
				} catch (...) {
					PERR("could not add parent service to registry");
					return 0;
				}
			}

			/* return parent service */
			return service;
		}

		void filter_session_args(const char *service,
		                         char *args, Genode::size_t args_len)
		{
			_labeling_policy.  filter_session_args(service, args, args_len);
			_priority_policy.  filter_session_args(service, args, args_len);
			_configfile_policy.filter_session_args(service, args, args_len);
		}

		bool announce_service(const char             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc)
		{
			PERR("unexpected announcement of service \"%s\" from child \"%s\"",
			     service_name, _name.unique);
			return false;
		}
};

#endif /* _CHILD_H_ */

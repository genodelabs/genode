/*
 * \brief  Init process
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <init/child.h>
#include <base/sleep.h>
#include <os/config.h>


namespace Init { bool config_verbose = false; }


/***************
 ** Utilities **
 ***************/

/**
 * Read priority-levels declaration from config
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


/**
 * Read affinity-space parameters from config
 *
 * If no affinity space is declared, construct a space with a single element,
 * width and height being 1. If only one of both dimensions is specified, the
 * other dimension is set to 1.
 */
inline Genode::Affinity::Space read_affinity_space()
{
	using namespace Genode;
	try {
		Xml_node node = config()->xml_node().sub_node("affinity-space");
		return Affinity::Space(node.attribute_value<unsigned long>("width",  1),
		                       node.attribute_value<unsigned long>("height", 1));
	} catch (...) {
		return Affinity::Space(1, 1); }
}


/**
 * Read parent-provided services from config
 */
inline void determine_parent_services(Genode::Service_registry *services)
{
	using namespace Genode;

	if (Init::config_verbose)
		printf("parent provides\n");

	Xml_node node = config()->xml_node().sub_node("parent-provides").sub_node("service");
	for (; ; node = node.next("service")) {

		char service_name[Genode::Service::MAX_NAME_LEN];
		node.attribute("name").value(service_name, sizeof(service_name));

		Parent_service *s = new (env()->heap()) Parent_service(service_name);
		services->insert(s);
		if (Init::config_verbose)
			printf("  service \"%s\"\n", service_name);

		if (node.is_last("service")) break;
	}
}


/********************
 ** Child registry **
 ********************/

namespace Init {

	typedef Genode::List<Genode::List_element<Child> > Child_list;

	class Child_registry : public Name_registry, Child_list
	{
		public:

			/**
			 * Register child
			 */
			void insert(Child *child)
			{
				Child_list::insert(&child->_list_element);
			}

			/**
			 * Unregister child
			 */
			void remove(Child *child)
			{
				Child_list::remove(&child->_list_element);
			}

			/**
			 * Start execution of all children
			 */
			void start()
			{
				Genode::List_element<Child> *curr = first();
				for (; curr; curr = curr->next())
					curr->object()->start();
			}

			/**
			 * Return any of the registered children, or 0 if no child exists
			 */
			Child *any()
			{
				return first() ? first()->object() : 0;
			}


			/*****************************
			 ** Name-registry interface **
			 *****************************/

			bool is_unique(const char *name) const
			{
				Genode::List_element<Child> const *curr = first();
				for (; curr; curr = curr->next())
					if (curr->object()->has_name(name))
						return false;

				return true;
			}

			Genode::Server *lookup_server(const char *name) const
			{
				Genode::List_element<Child> const *curr = first();
				for (; curr; curr = curr->next())
					if (curr->object()->has_name(name))
						return curr->object()->server();

				return 0;
			}
	};
}


int main(int, char **)
{
	using namespace Init;
	using namespace Genode;

	/* look for dynamic linker */
	try {
		static Rom_connection rom("ld.lib.so");
		Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	static Service_registry parent_services;
	static Service_registry child_services;
	static Child_registry   children;
	static Cap_connection   cap;

	/*
	 * Signal receiver for config changes
	 */
	Signal_receiver sig_rec;
	Signal_context  sig_ctx;
	config()->sigh(sig_rec.manage(&sig_ctx));

	for (;;) {

		try {
			config_verbose =
				config()->xml_node().attribute("verbose").has_value("yes"); }
		catch (...) { }

		try { determine_parent_services(&parent_services); }
		catch (...) { }

		/* determine default route for resolving service requests */
		Xml_node default_route_node("<empty/>");
		try {
			default_route_node =
			config()->xml_node().sub_node("default-route"); }
		catch (...) { }

		/* create children */
		try {
			Xml_node start_node = config()->xml_node().sub_node("start");
			for (;; start_node = start_node.next("start")) {

				try {
					children.insert(new (env()->heap())
					                Init::Child(start_node, default_route_node,
					                &children, read_prio_levels_log2(),
					                read_affinity_space(),
					                &parent_services, &child_services, &cap));
				}
				catch (Rom_connection::Rom_connection_failed) {
					/*
					 * The binary does not exist. An error message is printed
					 * by the Rom_connection constructor.
					 */
				}

				if (start_node.is_last("start")) break;
			}

			/* start children */
			children.start();
		}
		catch (Xml_node::Nonexistent_sub_node) {
			PERR("No children to start"); }
		catch (Xml_node::Invalid_syntax) {
			PERR("No children to start"); }
		catch (Init::Child::Child_name_is_not_unique) { }

		/*
		 * Respond to config changes at runtime
		 *
		 * If the config gets updated to a new version, we kill the current
		 * scenario and start again with the new config.
		 */

		/* wait for config change */
		sig_rec.wait_for_signal();

		/* kill all currently running children */
		while (children.any()) {
			Init::Child *child = children.any();
			children.remove(child);
			destroy(env()->heap(), child);
		}

		/* reset knowledge about parent services */
		parent_services.remove_all();

		/* reload config */
		try { config()->reload(); } catch (...) { }
	}

	return 0;
}


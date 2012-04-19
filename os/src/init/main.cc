/*
 * \brief  Init process
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
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


/**
 * Read parent-provided services from config file
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
			 * Start execution of all children
			 */
			void start() {
				Genode::List_element<Child> *curr = first();
				for (; curr; curr = curr->next())
					curr->object()->start();
			}


			/*****************************
			 ** Name-registry interface **
			 *****************************/

			bool is_unique(const char *name) const
			{
				Genode::List_element<Child> *curr = first();
				for (; curr; curr = curr->next())
					if (curr->object()->has_name(name))
						return false;

				return true;
			}

			Genode::Server *lookup_server(const char *name) const
			{
				Genode::List_element<Child> *curr = first();
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

	try {
		config_verbose =
			Genode::config()->xml_node().attribute("verbose").has_value("yes"); }
	catch (...) { }

	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	static Genode::Service_registry parent_services;
	static Genode::Service_registry child_services;
	static Child_registry           children;
	static Genode::Cap_connection   cap;

	try { determine_parent_services(&parent_services); }
	catch (...) { }

	/* determine default route for resolving service requests */
	Genode::Xml_node default_route_node("<empty/>");
	try {
		default_route_node =
			Genode::config()->xml_node().sub_node("default-route"); }
	catch (...) { }

	/* create children */
	try {
		Genode::Xml_node start_node = Genode::config()->xml_node().sub_node("start");
		for (;; start_node = start_node.next("start")) {

			children.insert(new (Genode::env()->heap())
							Child(start_node, default_route_node, &children,
								  read_prio_levels_log2(),
								  &parent_services, &child_services, &cap));

			if (start_node.is_last("start")) break;
		}
	}
	catch (Genode::Xml_node::Nonexistent_sub_node) {
		PERR("No children to start");
	}

	/* start children */
	children.start();

	Genode::sleep_forever();
	return 0;
}


/*
 * \brief  Launchpad child management
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2006-09-01
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <base/service.h>
#include <base/snprintf.h>
#include <base/attached_dataspace.h>
#include <launchpad/launchpad.h>

using namespace Genode;


/***************
 ** Launchpad **
 ***************/

Launchpad::Launchpad(Env &env, unsigned long initial_quota)
:
	_env(env),
	_initial_quota(initial_quota),
	_sliced_heap(_env.ram(), _env.rm())
{
	/* names of services provided by the parent */
	static const char *names[] = {

		/* core services */
		"RM", "PD", "CPU", "IO_MEM", "IO_PORT", "IRQ", "ROM", "LOG",

		/* services expected to got started by init */
		"Nitpicker", "Init", "Timer", "Block", "Nic", "Rtc", "Gpu", "Report",

		0 /* null-termination */
	};
	for (unsigned i = 0; names[i]; i++)
		new (_heap) Launchpad_child::Parent_service(_parent_services, names[i]);
}


/**
 * Check if a program with the specified name already exists
 */
bool Launchpad::_child_name_exists(Launchpad_child::Name const &name)
{
	Launchpad_child *c = _children.first();

	for ( ; c; c = c->List<Launchpad_child>::Element::next())
		if (c->name() == name)
			return true;

	return false;
}


/**
 * Create a unique name based on the filename
 *
 * If a program with the filename as name already exists, we add a counting
 * number as suffix.
 */
Launchpad_child::Name
Launchpad::_get_unique_child_name(Launchpad_child::Name const &binary_name)
{
	Lock::Guard lock_guard(_children_lock);

	if (!_child_name_exists(binary_name))
		return binary_name;

	for (unsigned cnt = 1; ; cnt++) {

		/* if such a program name does not exist yet, we are happy */
		Launchpad_child::Name const unique(binary_name, ".", cnt);
		if (!_child_name_exists(unique))
			return unique;
	}
}


/**
 * Process launchpad XML configuration
 */
void Launchpad::process_config(Genode::Xml_node config_node)
{
	using namespace Genode;

	/*
	 * Iterate through all entries of the config file and create
	 * launchpad entries as specified.
	 */
	config_node.for_each_sub_node("launcher", [&] (Xml_node node) {

		typedef Launchpad_child::Name Name;
		Name *name = new (_heap) Name(node.attribute_value("name", Name()));

		Number_of_bytes default_ram_quota =
			node.attribute_value("ram_quota", Number_of_bytes(0));

		Launchpad::Cap_quota const cap_quota { node.attribute_value("caps", 0UL) };

		/*
		 * Obtain configuration for the child
		 */
		Dataspace_capability config_ds;

		typedef String<128> Rom_name;

		if (node.has_sub_node("configfile")) {

			Rom_name const name =
				node.sub_node("configfile").attribute_value("name", Rom_name());

			Rom_connection &config_rom = *new (_heap) Rom_connection(_env, name.string());

			config_ds = config_rom.dataspace();
		}

		if (node.has_sub_node("config")) {

			Xml_node config_node = node.sub_node("config");

			/* allocate dataspace for config */
			size_t const size = config_node.size();
			config_ds = _env.ram().alloc(size);

			/* copy configuration into new dataspace */
			Attached_dataspace attached(_env.rm(), config_ds);
			memcpy(attached.local_addr<char>(), config_node.addr(), size);
		}

		/* add launchpad entry */
		add_launcher(*name, cap_quota, default_ram_quota, config_ds);
	});
}


Launchpad_child *Launchpad::start_child(Launchpad_child::Name const &binary_name,
                                        Cap_quota cap_quota, Ram_quota ram_quota,
                                        Dataspace_capability config_ds)
{
	log("starting ", binary_name, " with quota ", ram_quota);

	/* find unique name for new child */
	Launchpad_child::Name const unique_name = _get_unique_child_name(binary_name);
	log("using unique child name \"", unique_name, "\"");

	if (ram_quota.value > _env.ram().avail_ram().value) {
		warning("child's ram quota is higher than our available quota, using available quota");

		size_t const avail     = _env.ram().avail_ram().value;
		size_t const preserved = 256*1024;

		if (avail < preserved) {
			error("giving up, our own quota is too low (", avail, ")");
			return 0;
		}
		ram_quota = Ram_quota { avail - preserved };
	}

	size_t const avail_caps = _env.pd().avail_caps().value;

	if (cap_quota.value > avail_caps) {
		warning("child's cap quota (", cap_quota.value, ") exceeds the "
		        "number of available capabilities (", avail_caps, ")");

		size_t const preserved_caps = min(avail_caps, 25UL);

		cap_quota = Cap_quota { avail_caps - preserved_caps };
	}

	size_t metadata_size = 4096*16 + sizeof(Launchpad_child);

	if (metadata_size > ram_quota.value) {
		error("too low ram_quota to hold child metadata");
		return 0;
	}

	ram_quota = Ram_quota { ram_quota.value - metadata_size };

	try {
		Launchpad_child *c = new (&_sliced_heap)
			Launchpad_child(_env, _heap, unique_name, binary_name,
			                cap_quota, ram_quota,
			                _parent_services, _child_services, config_ds);

		Lock::Guard lock_guard(_children_lock);
		_children.insert(c);

		add_child(unique_name, ram_quota.value, *c, _heap);
		return c;

	} catch (...) {
		warning("failed to create child - unknown reason"); }

	return 0;
}


void Launchpad::exit_child(Launchpad_child &child)
{
	remove_child(child.name(), _heap);

	Lock::Guard lock_guard(_children_lock);
	_children.remove(&child);

	destroy(_sliced_heap, &child);
}

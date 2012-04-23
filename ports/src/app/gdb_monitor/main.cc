/*
 * \brief  GDB Monitor
 * \author Christian Prochaska
 * \date   2010-09-16
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/service.h>
#include <base/sleep.h>

#include <os/config.h>

#include <util/xml_node.h>

#include <rom_session/connection.h>
#include <ram_session/connection.h>
#include <cap_session/connection.h>

#include "app_child.h"
#include "rom.h"

using namespace Genode;
using namespace Gdb_monitor;

/*
 * Suppress messages of libc dummy functions
 */
extern "C" int _sigaction()   { return -1; }
extern "C" int  getpid()      { return -1; }
extern "C" int  sigprocmask() { return -1; }
extern "C" int _sigprocmask() { return -1; }


int main()
{
	/* look for dynamic linker */
	try {
		Rom_connection ldso_rom("ld.lib.so");
		Process::dynamic_linker(clone_rom(ldso_rom.dataspace()));
	} catch (...) {
		PDBG("ld.lib.so not found");
	}

	/* extract target filename from config file */

	static char filename[32] = "";

	try {
		config()->xml_node().sub_node("target").attribute("name").value(filename, sizeof(filename));
	} catch (Xml_node::Nonexistent_sub_node) {
		PERR("Error: Missing '<target>' sub node.");
		return -1;
	} catch (Xml_node::Nonexistent_attribute) {
		PERR("Error: Missing 'name' attribute of '<target>' sub node.");
		return -1;
	}

	/* extract target node from config file */
	Xml_node target_node = config()->xml_node().sub_node("target");

	/*
	 * preserve the configured amount of memory for gdb_monitor and give the
	 * remainder to the child
	 */
	Number_of_bytes preserved_ram_quota = 0;
	try {
		Xml_node preserve_node = config()->xml_node().sub_node("preserve");
		if (preserve_node.attribute("name").has_value("RAM"))
			preserve_node.attribute("quantum").value(&preserved_ram_quota);
		else
			throw Xml_node::Exception();
	} catch (...) {
		PERR("Error: could not find a valid <preserve> config node");
		return -1;
	}

	Number_of_bytes ram_quota = env()->ram_session()->avail() - preserved_ram_quota;

	/* start the application */
	char *unique_name = filename;
	Capability<Rom_dataspace> file_cap;
	try {
		static Rom_connection rom(filename, unique_name);
		file_cap = rom.dataspace();
	} catch (Rom_connection::Rom_connection_failed) {
		Genode::printf("Error: Could not access file \"%s\" from ROM service.\n", filename);
		return 0;
	}

	/* copy ELF image to writable dataspace */
	Genode::size_t elf_size = Dataspace_client(file_cap).size();
	Capability<Dataspace> elf_cap = clone_rom(file_cap);

	/* create ram session for child with some of our own quota */
	static Ram_connection ram;
	ram.ref_account(env()->ram_session_cap());
	env()->ram_session()->transfer_quota(ram.cap(), (Genode::size_t)ram_quota - elf_size);

	/* cap session for allocating capabilities for parent interfaces */
	static Cap_connection cap_session;

	static Service_registry parent_services;

	enum { CHILD_ROOT_EP_STACK = 4096 };
	static Rpc_entrypoint child_root_ep(&cap_session, CHILD_ROOT_EP_STACK,
	                                    "child_root_ep");

	new (env()->heap()) App_child(unique_name,
	                              elf_cap,
	                              ram.cap(),
	                              &cap_session,
	                              &parent_services,
	                              &child_root_ep,
	                              target_node);
	sleep_forever();

	return 0;
}

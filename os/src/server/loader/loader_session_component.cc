/*
 * \brief  Implementation of the Loader session interface
 * \author Christian Prochaska
 * \date   2009-10-06
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <root/root.h>
#include <dataspace/client.h>

/* local includes */
#include <loader_session_component.h>


using namespace Loader;
using namespace Genode;


Session_component::Session_component(size_t input_ds_size)
: _tar_rom(0),
  _tar_server_child_elf(0),
  _tar_server_child(0),
  _loader_child_elf(0),
  _loader_child(0)
{
	PDBG("available quota = %zd / %zd",
	     env()->ram_session()->avail(), env()->ram_session()->quota());
	try {
		if (input_ds_size > 0)
			_input_ds = env()->ram_session()->alloc(input_ds_size);
	} catch(Ram_session::Quota_exceeded) {
		PERR("Quota exceeded!");
	}

}


Session_component::~Session_component()
{
	destroy(env()->heap(), _loader_child);
	destroy(env()->heap(), _loader_child_elf);

	destroy(env()->heap(), _tar_server_child);
	destroy(env()->heap(), _tar_server_child_elf);

	destroy(env()->heap(), _tar_rom);

	env()->ram_session()->free(static_cap_cast<Ram_dataspace>(_input_ds));

	PDBG("available quota = %zd / %zd",
	     env()->ram_session()->avail(), env()->ram_session()->quota());
}


void Session_component::start(Start_args const &args,
                              int max_width, int max_height,
                              Alarm::Time timeout,
                              Name const &name)
{
	char *input_ds_addr = env()->rm_session()->attach(_input_ds);

	if (!input_ds_addr)
		throw Start_failed();

	if (!args.is_valid_string())
		throw Invalid_args();

	_ram_quota = Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);

	enum { LOADER_RAM_QUOTA = 512*1024 };
	enum { TAR_SERVER_RAM_QUOTA = 512*1024 };

	/*
	 * In the demo scenario, the ram amount stated by the "ram_quota"
	 * argument is meant for both the loader and the child, so some memory
	 * gets reserved for the loader here.
	 */
	if (_ram_quota)
		_ram_quota -= LOADER_RAM_QUOTA;

	if ((input_ds_addr[0] != 0x7f) ||
		(input_ds_addr[1] != 'E') ||
		(input_ds_addr[2] != 'L') ||
		(input_ds_addr[3] != 'F')) {

		/* dataspace presumably contains tar archive */

		/*
		 * TAR server child
		 */

		try {
			_tar_server_child_elf = new (env()->heap()) Rom_connection("tar_rom", "tar_rom");
			_tar_server_child_elf_ds = _tar_server_child_elf->dataspace();
		} catch (Rom_connection::Rom_connection_failed) {
			printf("Error: Could not access file \"%s\" from ROM service.\n", "tar_rom");
			throw Rom_access_failed();
		}

		_tar_server_child = new (env()->heap())
			Tar_server_child("tar_rom",
			                 _tar_server_child_elf_ds,
			                 TAR_SERVER_RAM_QUOTA,
			                 &_cap_session,
			                 &_tar_server_child_parent_services,
			                 _input_ds);

		/*
		 * Deduce quota donations for RAM, RM, and CPU from available quota
		 */
		_ram_quota -= (Tar_server_child::DONATIONS + TAR_SERVER_RAM_QUOTA);

		try {
			_loader_child_elf = new (env()->heap()) Rom_connection("init", "init");
			_loader_child_elf_ds = _loader_child_elf->dataspace();
		} catch (Rom_connection::Rom_connection_failed) {
			printf("Error: Could not access file \"%s\" from ROM service.\n", "init");
			throw Rom_access_failed();
		}

	} else {
		/* dataspace contains ELF binary */
		_loader_child_elf_ds = _input_ds;
	}

	env()->rm_session()->detach(input_ds_addr);

	/*
	 * Loader child
	 */

	_loader_child = new (env()->heap())
		Child(_tar_server_child ? "init" : name.string(),
		      _loader_child_elf_ds,
		      &_cap_session,
		      _ram_quota,
		      &_loader_child_parent_services,
		      &_ready_sem,
		      max_width,
		      max_height,
		      _tar_server_child ? _tar_server_child->tar_server_root()
		                        : Root_capability());

	try {
		_ready_sem.down(timeout);
	} catch (Genode::Timeout_exception) {
		throw Plugin_start_timed_out();
	} catch (Genode::Nonblocking_exception) {
		throw Plugin_start_timed_out();
	}
}


Nitpicker::View_capability Session_component::view(int *w, int *h, int *buf_x, int *buf_y)
{
	return _loader_child->view(w, h, buf_x, buf_y);
}

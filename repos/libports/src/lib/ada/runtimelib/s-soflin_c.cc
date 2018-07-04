/*
 * \brief Dummy implementation for Ada softlinks
 * \author Johannes Kliemann
 * \date 2018-04-16
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

extern "C" {

	void system__soft_links__get_current_excep()
	{
		Genode::warning(__func__, " not implemented");
	}

	void system__soft_links__get_gnat_exception()
	{
		Genode::warning(__func__, " not implemented");
	}

	void system__soft_links__get_jmpbuf_address_soft()
	{
		Genode::warning(__func__, " not implemented");
	}

	void system__soft_links__set_jmpbuf_address_soft()
	{
		Genode::warning(__func__, " not implemented");
	}

	void system__soft_links__lock_task()
	{
		Genode::warning(__func__, " not implemented");
	}

	void system__soft_links__unlock_task()
	{
		Genode::warning(__func__, " not implemented");
	}

}

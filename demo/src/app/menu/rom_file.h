/*
 * \brief  Interface to access binary data at local address space
 * \author Norman Feske
 * \date   2010-10-28
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROM_FILE_H_
#define _ROM_FILE_H_

#include <rom_session/connection.h>
#include <base/env.h>

class Rom_file
{
	private:

		Genode::Rom_connection _rom;
		void *_local_addr;

	public:

		Rom_file(const char *name):
			_rom(name),
			_local_addr(Genode::env()->rm_session()->attach(_rom.dataspace()))
		{ }

		~Rom_file()
		{
			Genode::env()->rm_session()->detach(_local_addr);
		}

		void *local_addr() const { return _local_addr; }
};

#endif /* _ROM_FILE_H_ */

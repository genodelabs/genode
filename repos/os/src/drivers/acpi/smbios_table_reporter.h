/*
 * \brief  Finding and reporting an SMBIOS table as is (plain data)
 * \author Martin Stein
 * \date   2019-07-09
 */

 /*
  * Copyright (C) 2019 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

#ifndef _SMBIOS_TABLE_REPORTER_H_
#define _SMBIOS_TABLE_REPORTER_H_

/* Genode includes */
#include <os/reporter.h>
#include <base/allocator.h>

namespace Genode { class Smbios_table_reporter; }

class Genode::Smbios_table_reporter
{
	private:

		Constructible<Reporter> _reporter { };

	public:

		Smbios_table_reporter(Env       &env,
		                      Allocator &alloc);
};

#endif /* _SMBIOS_TABLE_REPORTER_H_ */

/*
 * \brief  Lookup Intel opregion and report it as is (plain data)
 * \author Alexander Boettcher
 * \date   2022-05-25
 */

 /*
  * Copyright (C) 2022 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

#ifndef _INTEL_OPREGION_REPORTER_H_
#define _INTEL_OPREGION_REPORTER_H_

/* Genode includes */
#include <os/reporter.h>

namespace Acpi {
	class Intel_opregion;
	using namespace Genode;
}

class Acpi::Intel_opregion
{
	private:

		Constructible<Reporter>  _report { };

		void generate_report(Env &env, addr_t, addr_t);

	public:

		Intel_opregion(Env &env, addr_t phys_base, addr_t size)
		{
			generate_report(env, phys_base, size);
		}
};

#endif /* _INTEL_OPREGION_REPORTER_H_ */

/*
 * \brief  Interface to ACPI
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-02-25
 */

 /*
  * Copyright (C) 2009-2017 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

#ifndef _ACPI_H_
#define _ACPI_H_

/* Genode includes */
#include <base/env.h>
#include <base/allocator.h>
#include <util/xml_node.h>


namespace Acpi
{
	/**
	 * Generate report rom
	 */
	void generate_report(Genode::Env&, Genode::Allocator&, Genode::Xml_node const&);
}

#endif /* _ACPI_H_ */


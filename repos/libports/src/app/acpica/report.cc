/*
 * \brief  Generate XML report
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2018-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <os/reporter.h>

#include "util.h"

using namespace Acpica;

void Acpica::generate_suspend_report(Reporter::Xml_generator &xml,
                                     String<32> const &state)
{
	static unsigned version = 0;

	xml.attribute("version"  , version++);
	xml.attribute("complete" , state);

	for (unsigned sleep_state = 1; sleep_state < ACPI_S_STATE_COUNT; sleep_state ++) {
		UINT8 slp_typa {};
		UINT8 slp_typb {};

		ACPI_STATUS const result = AcpiGetSleepTypeData (sleep_state,
		                                                 &slp_typa,
		                                                 &slp_typb);

		Genode::String<4> const state_name("S", sleep_state);
		xml.node(state_name.string(), [&] () {
			xml.attribute("supported", result == AE_OK);
			if (result == AE_OK) {
				xml.attribute("SLP_TYPa", slp_typa);
				xml.attribute("SLP_TYPb", slp_typb);
			}
		});
	}
}

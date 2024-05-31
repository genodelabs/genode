/*
 * \brief  Generation specific workarounds
 * \author Sebastian Sumpf
 * \data   2021-09-21
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#include <mmio.h>

namespace Igd {
	static void apply_workarounds(Mmio &mmio, unsigned generation);
}

void Igd::apply_workarounds(Mmio &mmio, unsigned generation)
{
	/*
	 * WaEnableGapsTsvCreditFix
	 *
	 * Sets the "GAPS TSV Credit fix ENABLE" of the RC arbiter bit that should be
	 * set by BIOS for GEN9+. If this bit is not set, GPU will hang arbitrary
	 * during batch buffer execution
	 */
	if (generation >= 9)
		mmio.write<Mmio::Arbiter_control::Gaps_tsv_enable>(1);
}

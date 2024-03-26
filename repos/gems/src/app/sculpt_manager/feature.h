/*
 * \brief  Compile-time feature selection
 * \author Norman Feske
 * \date   2022-11-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FEATURE_H_
#define _FEATURE_H_

#include <types.h>

namespace Sculpt { struct Feature; };


struct Sculpt::Feature
{
	/* show the '+' botton at the graph for opening the deploy popup dialog */
	static constexpr bool PRESENT_PLUS_MENU = true;

	/* allow the browsing of file systems via the inspect view */
	static constexpr bool INSPECT_VIEW = true;
};

#endif /* _FEATURE_H_ */

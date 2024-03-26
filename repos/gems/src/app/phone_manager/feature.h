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
	static constexpr bool PRESENT_PLUS_MENU = false;
	static constexpr bool INSPECT_VIEW = false;
};

#endif /* _FEATURE_H_ */

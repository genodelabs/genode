/*
 * \brief  Return type for progressed or stalled condition
 * \author Norman Feske
 * \date   2026-03-06
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__PROGRESS_H_
#define _INCLUDE__UTIL__PROGRESS_H_

namespace Genode {

	struct Progress
	{
		bool progressed;
		bool stalled() const { return !progressed; }
	};

	static constexpr Progress PROGRESSED { .progressed = true  };
	static constexpr Progress STALLED    { .progressed = false };
}

#endif /* _INCLUDE__UTIL__PROGRESS_H_ */

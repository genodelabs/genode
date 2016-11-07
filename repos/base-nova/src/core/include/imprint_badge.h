/*
 * \brief  Utility to imprint a badge into a NOVA portal
 * \author Norman Feske
 * \date   2016-03-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__IMPRINT_BADGE_H_
#define _CORE__INCLUDE__IMPRINT_BADGE_H_

/* NOVA includes */
#include <nova/syscalls.h>

static inline bool imprint_badge(unsigned long pt_sel, unsigned long badge)
{
	using namespace Nova;

	/* assign badge to portal */
	if (pt_ctrl(pt_sel, badge) != NOVA_OK)
		return false;

	/* disable PT_CTRL permission to prevent subsequent imprint attempts */
	revoke(Obj_crd(pt_sel, 0, Obj_crd::RIGHT_PT_CTRL));
	return true;
}

#endif /* _CORE__INCLUDE__IMPRINT_BADGE_H_ */

/**
 * \brief  DTB access helper
 * \author Josef Soentgen
 * \date   2023-04-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _DTB_HELPER_H_
#define _DTB_HELPER_H_

/* Genode includes */
#include <base/env.h>


struct Dtb_helper
{
	Genode::Env &_env;

	Dtb_helper(Genode::Env &env);

	void *dtb_ptr();
};

#endif /* _DTB_HELPER_H_ */

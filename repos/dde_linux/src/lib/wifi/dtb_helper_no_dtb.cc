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


#include "dtb_helper.h"


Dtb_helper::Dtb_helper(Genode::Env &env) : _env { env } { }


void *Dtb_helper::dtb_ptr() { return nullptr; }

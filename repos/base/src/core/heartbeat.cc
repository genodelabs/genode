/*
 * \brief  Omit heartbeat monitoring because core has no parent
 * \author Norman Feske
 * \date   2018-11-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>

/* base-internal includes */
#include <base/internal/globals.h>


void Genode::init_heartbeat_monitoring(Env &) { }


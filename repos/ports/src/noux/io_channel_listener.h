/*
 * \brief  IO channel listener
 * \author Christian Prochaska
 * \date   2014-01-23
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__IO_CHANNEL_LISTENER__H_
#define _NOUX__IO_CHANNEL_LISTENER__H_

/* Genode includes */
#include <util/list.h>

/* Noux includes */
#include <interrupt_handler.h>

namespace Noux { typedef List_element<Interrupt_handler> Io_channel_listener; }

#endif /* _NOUX__IO_CHANNEL_LISTENER__H_ */


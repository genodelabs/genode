/*
 * \brief  Dummy input I/O channel to be used for non-interactive init
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__DUMMY_INPUT_IO_CHANNEL_H_
#define _NOUX__DUMMY_INPUT_IO_CHANNEL_H_

/* Noux includes */
#include <io_channel.h>

namespace Noux {

	class Sysio;

	struct Dummy_input_io_channel : public Io_channel
	{ };
}

#endif /* _NOUX__DUMMY_INPUT_IO_CHANNEL_H_ */

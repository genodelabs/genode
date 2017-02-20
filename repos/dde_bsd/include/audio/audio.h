/*
 * \brief  Audio library interface
 * \author Josef Soentgen
 * \date   2014-12-27
 *
 * This header declares the private Audio namespace. It contains
 * functions called by the driver frontend that are implemented
 * by the driver library.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AUDIO__AUDIO_H_
#define _AUDIO__AUDIO_H_

/* Genode includes */
#include <base/env.h>
#include <util/xml_node.h>


/*****************************
 ** private Audio namespace **
 *****************************/

namespace Audio_out {

	enum Channel_number { LEFT, RIGHT, MAX_CHANNELS, INVALID = MAX_CHANNELS };
}


namespace Audio_in {

	enum Channel_number { LEFT, MAX_CHANNELS, INVALID = MAX_CHANNELS };
}


namespace Audio {

	void update_config(Genode::Env &, Genode::Xml_node);

	void init_driver(Genode::Env &, Genode::Allocator &, Genode::Xml_node);

	bool driver_active();

	void play_sigh(Genode::Signal_context_capability cap);

	void record_sigh(Genode::Signal_context_capability cap);

	int play(short *data, Genode::size_t size);

	int record(short *data, Genode::size_t size);
}

#endif /* _AUDIO__AUDIO_H_ */

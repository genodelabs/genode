/*
 * \brief  Audio-in session capability type
 * \author Josef Soentgen
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_IN_SESSION__CAPABILITY_H_
#define _INCLUDE__AUDIO_IN_SESSION__CAPABILITY_H_

#include <session/capability.h>

namespace Audio_in {

	/*
	 * We cannot include 'audio_in_session/audio_in_session.h'
	 * because this file relies on the 'Audio_in::Session_capability' type.
	 */
	class Session;
	typedef Genode::Capability<Session> Session_capability;
}

#endif /* _INCLUDE__AUDIO_IN_SESSION__CAPABILITY_H_ */

/*
 * \brief  Audio-out session capability type
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2009-12-01
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__CAPABILITY_H_
#define _INCLUDE__AUDIO_OUT_SESSION__CAPABILITY_H_

#include <session/capability.h>

namespace Audio_out {

	/*
	 * We cannot include 'audio_out_session/audio_out_session.h'
	 * because this file relies on the 'Audio_out::Session_capability' type.
	 */
	class Session;
	typedef Genode::Capability<Session> Session_capability;
}

#endif /* _INCLUDE__AUDIO_OUT_SESSION__CAPABILITY_H_ */

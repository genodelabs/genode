/*
 * \brief  Client-side Zynq VDMA session interface
 * \author Mark Albers
 * \date   2015-04-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VDMA_SESSION_H__CLIENT_H_
#define _INCLUDE__VDMA_SESSION_H__CLIENT_H_

#include <vdma_session/zynq/capability.h>
#include <base/rpc_client.h>

#define S2MM false
#define MM2S true

namespace Vdma {

	struct Session_client : Genode::Rpc_client<Session>
	{
		explicit Session_client(Session_capability session)
		: Genode::Rpc_client<Session>(session) { }

        bool setConfig(Genode::uint32_t data, bool isMM2S) { return call<Rpc_setConfig>(data, isMM2S); }

        bool setStride(Genode::uint16_t data, bool isMM2S) { return call<Rpc_setStride>(data, isMM2S); }

        bool setWidth(Genode::uint16_t data, bool isMM2S) { return call<Rpc_setWidth>(data, isMM2S); }

        bool setHeight(Genode::uint16_t data, bool isMM2S) { return call<Rpc_setHeight>(data, isMM2S); }

        bool setAddr(Genode::uint32_t data, bool isMM2S) { return call<Rpc_setAddr>(data, isMM2S); }
	};
}

#endif /* _INCLUDE__VDMA_SESSION_H__CLIENT_H_ */

/*
 * \brief  Zynq VDMA session interface
 * \author Mark Albers
 * \date   2015-04-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VDMA_SESSION__VDMA_SESSION_H_
#define _INCLUDE__VDMA_SESSION__VDMA_SESSION_H_

#include <base/signal.h>
#include <session/session.h>

namespace Vdma {

	struct Session : Genode::Session
	{
        static const char *service_name() { return "Vdma"; }

		virtual ~Session() { }

        virtual bool setConfig(Genode::uint32_t data, bool isMM2S) = 0;

        virtual bool setStride(Genode::uint16_t data, bool isMM2S) = 0;

        virtual bool setWidth(Genode::uint16_t data, bool isMM2S) = 0;

        virtual bool setHeight(Genode::uint16_t data, bool isMM2S) = 0;

        virtual bool setAddr(Genode::uint32_t data, bool isMM2S) = 0;


		/*******************
		 ** RPC interface **
		 *******************/

        GENODE_RPC(Rpc_setConfig, bool, setConfig, Genode::uint32_t, bool);
        GENODE_RPC(Rpc_setStride, bool, setStride, Genode::uint16_t, bool);
        GENODE_RPC(Rpc_setWidth, bool, setWidth, Genode::uint16_t, bool);
        GENODE_RPC(Rpc_setHeight, bool, setHeight, Genode::uint16_t, bool);
        GENODE_RPC(Rpc_setAddr, bool, setAddr, Genode::uint32_t, bool);

        GENODE_RPC_INTERFACE(Rpc_setConfig, Rpc_setStride, Rpc_setWidth,
                             Rpc_setHeight, Rpc_setAddr);
    };
}

#endif /* _INCLUDE__VDMA_SESSION__VDMA_SESSION_H_ */

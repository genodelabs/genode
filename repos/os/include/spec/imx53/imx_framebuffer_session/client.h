/*
 * \brief  Client-side i.MX53 specific framebuffer interface
 * \author Stefan Kalkowski
 * \date   2013-02-26
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IMX_FRAMEBUFFER_SESSION__CLIENT_H_
#define _INCLUDE__IMX_FRAMEBUFFER_SESSION__CLIENT_H_

#include <framebuffer_session/capability.h>
#include <imx_framebuffer_session/imx_framebuffer_session.h>
#include <base/rpc_client.h>

namespace Framebuffer { struct Imx_client; }


struct Framebuffer::Imx_client : Genode::Rpc_client<Imx_session>
{
	explicit Imx_client(Capability<Imx_session> session)
	: Genode::Rpc_client<Imx_session>(session) { }

	Genode::Dataspace_capability dataspace() override {
		return call<Rpc_dataspace>(); }

	Mode mode() const override { return call<Rpc_mode>(); }

	void mode_sigh(Genode::Signal_context_capability sigh) override {
		call<Rpc_mode_sigh>(sigh); }

	void sync_sigh(Genode::Signal_context_capability sigh) override {
		call<Rpc_sync_sigh>(sigh); }

	void refresh(int x, int y, int w, int h) override {
		call<Rpc_refresh>(x, y, w, h); }

	void overlay(Genode::addr_t phys_addr, int x, int y, int alpha) override {
		call<Rpc_overlay>(phys_addr, x, y, alpha); }
};

#endif /* _INCLUDE__IMX_FRAMEBUFFER_SESSION__CLIENT_H_ */

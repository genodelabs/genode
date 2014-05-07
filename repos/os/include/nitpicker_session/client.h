/*
 * \brief  Client-side nitpicker session interface
 * \author Norman Feske
 * \date   2006-08-23
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_SESSION__CLIENT_H_
#define _INCLUDE__NITPICKER_SESSION__CLIENT_H_

#include <nitpicker_session/capability.h>
#include <base/rpc_client.h>

namespace Nitpicker { struct Session_client; }


struct Nitpicker::Session_client : public Genode::Rpc_client<Session>
{
	explicit Session_client(Session_capability session)
	: Rpc_client<Session>(session) { }

	Framebuffer::Session_capability framebuffer_session() {
		return call<Rpc_framebuffer_session>(); }

	Input::Session_capability input_session() {
		return call<Rpc_input_session>(); }

	View_capability create_view(View_capability parent = View_capability()) {
		return call<Rpc_create_view>(parent); }

	void destroy_view(View_capability view) {
		call<Rpc_destroy_view>(view); }

	int background(View_capability view) {
		return call<Rpc_background>(view); }

	Framebuffer::Mode mode() {
		return call<Rpc_mode>(); }

	void buffer(Framebuffer::Mode mode, bool alpha) {
		call<Rpc_buffer>(mode, alpha); }
};

#endif /* _INCLUDE__NITPICKER_SESSION__CLIENT_H_ */

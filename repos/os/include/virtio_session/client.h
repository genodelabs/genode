/*
 * \brief  Client-side VirtIO bus session interface
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <virtio_session/capability.h>
#include <virtio_device/capability.h>
#include <base/rpc_client.h>

namespace Virtio { struct Session_client; }

struct Virtio::Session_client : Genode::Rpc_client<Virtio::Session>
{
	explicit Session_client(Virtio::Session_capability session)
	: Genode::Rpc_client<Session>(session) { }

	Device_capability first_device(Device_type type) override {
		return call<Rpc_first_device>(type); }

	Device_capability next_device(Device_capability prev_device) override {
		return call<Rpc_next_device>(prev_device); }

	void release_device(Device_capability device) override {
		call<Rpc_release_device>(device); }
};

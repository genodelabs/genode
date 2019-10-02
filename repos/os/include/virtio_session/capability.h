/*
 * \brief  VirtIO bus capability type
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

#include <base/capability.h>
#include <virtio_session/virtio_session.h>

namespace Virtio { typedef Genode::Capability<Virtio::Session> Session_capability; }

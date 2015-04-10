/*
 * \brief  Generic implementation parts of the irq framework which are
 *         implemented platform specifically, e.g. base-hw.
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <irq_session/connection.h>

void Genode::Irq_session_client::ack_irq()
{
	call<Rpc_ack_irq>();
}

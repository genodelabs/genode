/*
 * \brief  Genode environment for ACPICA library
 * \author Christian Helmuth
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/reconstructible.h>
#include <acpica/acpica.h>
#include <platform_session/client.h>

#include "env.h"


namespace Acpica { struct Env; }


struct Acpica::Env
{
	Genode::Env       &env;
	Genode::Allocator &heap;

	Wait_acpi_ready const wait_acpi_ready;

	Genode::Parent::Service_name announce_for_acpica {
		wait_acpi_ready.enabled ? "Acpi" : Platform::Session::service_name() };

	Genode::Parent::Client parent_client;

	Genode::Id_space<Genode::Parent::Client>::Element id_space_element {
		parent_client, env.id_space() };

	Genode::Capability<Platform::Session> cap {
		Genode::reinterpret_cap_cast<Platform::Session>(
			env.session(announce_for_acpica,
			            id_space_element.id(),
			            "ram_quota=36K", Genode::Affinity())) };

	Platform::Client platform { cap };

	Env(Genode::Env &env, Genode::Allocator &heap, Wait_acpi_ready wait_acpi_ready)
	: env(env), heap(heap), wait_acpi_ready(wait_acpi_ready) { }
};

static Genode::Constructible<Acpica::Env> instance;


Genode::Allocator & Acpica::heap()     { return instance->heap; }
Genode::Env       & Acpica::env()      { return instance->env; }
Platform::Client  & Acpica::platform() { return instance->platform; }


void Acpica::init(Genode::Env &env, Genode::Allocator &heap,
                  Wait_acpi_ready wait_acpi_ready)
{
	instance.construct(env, heap, wait_acpi_ready);
}

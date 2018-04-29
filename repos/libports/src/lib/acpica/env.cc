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
	bool use_platform_drv;

	Genode::Parent::Service_name announce_for_acpica {
		wait_acpi_ready.enabled ? "Acpi" : Platform::Session::service_name() };

	Genode::Parent::Client parent_client;

	Genode::Id_space<Genode::Parent::Client>::Element id_space_element {
		parent_client, env.id_space() };

	Genode::Constructible<Genode::Capability<Platform::Session>> cap;
	Genode::Constructible<Platform::Client> platform;

	Env(Genode::Env &env, Genode::Allocator &heap,
	    Wait_acpi_ready wait_acpi_ready)
	:
		env(env), heap(heap), wait_acpi_ready(wait_acpi_ready),
		use_platform_drv(!wait_acpi_ready.enabled)
	{ }
};

static Genode::Constructible<Acpica::Env> instance;


Genode::Allocator & Acpica::heap()     { return instance->heap; }
Genode::Env       & Acpica::env()      { return instance->env; }
Platform::Client  & Acpica::platform()
{
	if (!instance->cap.constructed()) {
		instance->cap.construct(Genode::reinterpret_cap_cast<Platform::Session>(
			instance->env.session(instance->announce_for_acpica,
			                      instance->id_space_element.id(),
			                      "ram_quota=36K", Genode::Affinity())));

		instance->platform.construct(*instance->cap);
	}
	return *instance->platform;
}
bool Acpica::platform_drv() { return instance->use_platform_drv; }
void Acpica::use_platform_drv() { instance->use_platform_drv = true; }


void Acpica::init(Genode::Env &env, Genode::Allocator &heap,
                  Wait_acpi_ready const wait_acpi_ready,
                  Act_as_acpi_drv const act_as_acpi_drv)
{
	instance.construct(env, heap, wait_acpi_ready);

	/* if not running as acpi_drv, block until original acpi_drv is done */
	if (!act_as_acpi_drv.enabled)
		platform();
}

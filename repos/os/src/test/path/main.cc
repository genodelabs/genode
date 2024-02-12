/*
 * \brief  Tests for the 'Genode::Path' class
 * \author Christian Prochaska
 * \date   2024-02-08
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <os/path.h>

using namespace Genode;

void Component::construct(Env &)
{
	log("--- 'Genode::Path' test ---");

	log("");
	log("testing removal of superfluous slashes");
	log("");

	log("'//dir1///' -> '", Path<16>("//dir1///"), "'");

	log("");
	log("testing removal of superfluous dot-slashes");
	log("");

	log("'/./dir1/././' -> '", Path<16>("/./dir1/././"), "'");
	log("'./dir1/././' -> '",  Path<16>("dir1/././", "./"), "'");

	log("");
	log("testing removal of double dot dirs");
	log("");

	log("'/..' -> '",           Path<16>("/.."), "'");
	log("'/dir1/..' -> '",      Path<16>("/dir1/.."), "'");
	log("'/dir1/../..' -> '",   Path<16>("/dir1/../.."), "'");
	log("'/dir1/dir2/..' -> '", Path<16>("/dir1/dir2/.."), "'");
	log("'/dir1/../dir2' -> '", Path<16>("/dir1/../dir2"), "'");
	log("'/../dir1' -> '",      Path<16>("/../dir1"), "'");
	log("'/../../dir1' -> '",   Path<16>("/../../dir1"), "'");
	log("'/../dir1/..' -> '",   Path<16>("/../dir1/.."), "'");

	log("'..' -> '",            Path<16>("", ".."), "'");
	log("'dir1/..' -> '",       Path<16>("..", "dir1"), "'");
	log("'dir1/../..' -> '",    Path<16>("../..", "dir1"), "'");
	log("'dir1/dir2/..' -> '",  Path<16>("dir2/..", "dir1"), "'");
	log("'dir1/../dir2' -> '",  Path<16>("../dir2", "dir1"), "'");
	log("'../dir1' -> '",       Path<16>("dir1", ".."), "'");
	log("'../../dir1' -> '",    Path<16>("../dir1", ".."), "'");
	log("'../dir1/..' -> '",    Path<16>("dir1/..", ".."), "'");

	log("");
	log("testing removal of trailing dot");
	log("");

	log("'/dir1/.' -> '", Path<16>("/dir1/."), "'");

	log("");
	log("--- 'Genode::Path' test finished ---");
}

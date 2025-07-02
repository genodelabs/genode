/*
 * \brief  Interface for querying information about the depot
 * \author Norman Feske
 * \date   2019-02-22
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DEPOT_QUERY_H_
#define _DEPOT_QUERY_H_

#include "types.h"

namespace Sculpt {

	struct Depot_query;

	static inline bool blueprint_missing        (Node const &, Path const &);
	static inline bool blueprint_any_missing    (Node const &);
	static inline bool blueprint_rom_missing    (Node const &, Path const &);
	static inline bool blueprint_any_rom_missing(Node const &);
}


struct Sculpt::Depot_query : Interface
{
	struct Version { unsigned value; };

	virtual Version depot_query_version() const = 0;

	virtual void trigger_depot_query() = 0;
};


static inline bool Sculpt::blueprint_missing(Node const &blueprint, Path const &path)
{
	bool result = false;
	blueprint.for_each_sub_node("missing", [&] (Node const &missing) {
		if (missing.attribute_value("path", Path()) == path)
			result = true; });

	return result;
}


static inline bool Sculpt::blueprint_any_missing(Node const &blueprint)
{
	return blueprint.has_sub_node("missing");
}


/**
 * Return true if one or more ROMs of the pkg 'path' are missing from the
 * blueprint
 *
 * If 'path' is an invalid string, all pkgs of the blueprint are checked.
 */
static inline bool Sculpt::blueprint_rom_missing(Node const &blueprint, Path const &path)
{
	bool result = false;
	blueprint.for_each_sub_node("pkg", [&] (Node const &pkg) {

		/* skip pkgs that we are not interested in */
		if (path.valid() && pkg.attribute_value("path", Path()) != path)
			return;

		pkg.for_each_sub_node("missing_rom", [&] (Node const &missing_rom) {

			/* ld.lib.so is always taken from the base system */
			using Label = String<64>;
			Label const label = missing_rom.attribute_value("label", Label());
			if (label == "ld.lib.so")
				return;

			/* some ingredient is not extracted yet, or actually missing */
			result = true;
		});
	});
	return result;
}


static inline bool Sculpt::blueprint_any_rom_missing(Node const &blueprint)
{
	return blueprint_rom_missing(blueprint, Path());
}

#endif /* _DEPOT_QUERY_H_ */

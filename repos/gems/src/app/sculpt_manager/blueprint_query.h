/*
 * \brief  Interface for querying blueprints about the depot
 * \author Norman Feske
 * \date   2026-03-18
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BLUEPRINT_QUERY_H_
#define _BLUEPRINT_QUERY_H_

#include "types.h"

namespace Sculpt {

	struct Blueprint_query;

	static inline bool blueprint_missing        (Node const &, Path const &);
	static inline bool blueprint_any_missing    (Node const &);
	static inline bool blueprint_rom_missing    (Node const &, Path const &);
	static inline bool blueprint_any_rom_missing(Node const &);
}


struct Sculpt::Blueprint_query : Interface
{
	struct Version { unsigned value; };

	virtual Version blueprint_query_version() const = 0;

	virtual void query_blueprint() = 0;
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
			using Name = String<64>;
			Name const name = missing_rom.attribute_value("name",
			                     missing_rom.attribute_value("label", Name()));
			if (name == "ld.lib.so")
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

#endif /* _BLUEPRINT_QUERY_H_ */

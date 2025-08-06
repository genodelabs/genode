/*
 * \brief  Linker configuration
 * \author Norman Feske
 * \date   2019-08-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CONFIG_H_
#define _INCLUDE__CONFIG_H_

#include <util/reconstructible.h>
#include <base/attached_rom_dataspace.h>

namespace Linker { class Config; }


class Linker::Config : Noncopyable
{
	private:

		Attached_rom_dataspace const _config;

	public:

		Bind const bind = _config.node().attribute_value("ld_bind_now", false)
		                ? BIND_NOW : BIND_LAZY;

		bool const verbose      = _config.node().attribute_value("ld_verbose",     false);
		bool const check_ctors  = _config.node().attribute_value("ld_check_ctors", true);
		bool const generate_xml = _config.node().attribute_value("generate_xml",   true);

		Config(Env &env) : _config(env, "config") { }

		using Rom_name = String<100>;

		/**
		 * Call fn for each library specified in the configuration
		 *
		 * The functor 'fn' is called with 'Rom_name', 'Keep' as arguments.
		 */
		void for_each_library(auto const &fn) const
		{
			_config.node().with_optional_sub_node("ld", [&] (Node const &ld) {

				ld.for_each_sub_node("library", [&] (Node const &lib) {

					Rom_name const rom = lib.attribute_value("rom", Rom_name());

					Keep const keep = lib.attribute_value("keep", false)
					                ? DONT_KEEP : KEEP;

					fn(rom, keep);
				});
			});
		}
};

#endif /* _INCLUDE__CONFIG_H_ */

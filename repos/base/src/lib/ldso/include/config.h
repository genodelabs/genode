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

		/*
		 * Helper to transparently handle the case where no "config" ROM is
		 * available.
		 */
		struct Xml_config : Xml_node
		{
			Constructible<Attached_rom_dataspace> _rom { };

			Xml_config(Env &env) : Xml_node("<empty/>")
			{
				try {
					_rom.construct(env, "config");
					static_cast<Xml_node &>(*this) = _rom->xml();
				}
				catch (...) { }
			}
		};

		Xml_config const _config;

		Bind const _bind = _config.attribute_value("ld_bind_now", false)
		                 ? BIND_NOW : BIND_LAZY;

		bool const _verbose     = _config.attribute_value("ld_verbose",     false);
		bool const _check_ctors = _config.attribute_value("ld_check_ctors", true);

	public:

		Config(Env &env) : _config(env) { }

		Bind bind()        const { return _bind; }
		bool verbose()     const { return _verbose; }
		bool check_ctors() const { return _check_ctors; }

		typedef String<100> Rom_name;

		/**
		 * Call fn for each library specified in the configuration
		 *
		 * The functor 'fn' is called with 'Rom_name', 'Keep' as arguments.
		 */
		template <typename FN>
		void for_each_library(FN const &fn) const
		{
			_config.with_sub_node("ld", [&] (Xml_node ld) {

				ld.for_each_sub_node("library", [&] (Xml_node lib) {

					Rom_name const rom = lib.attribute_value("rom", Rom_name());

					Keep const keep = lib.attribute_value("keep", false)
					                ? DONT_KEEP : KEEP;

					fn(rom, keep);
				});
			});
		}
};

#endif /* _INCLUDE__CONFIG_H_ */

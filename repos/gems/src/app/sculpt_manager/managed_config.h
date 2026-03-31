/*
 * \brief  Management of configurations that can be overridden by the user
 * \author Norman Feske
 * \date   2018-05-25
 */

/*
 * Copyright (C) 2018-2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MANAGED_CONFIG_H_
#define _MANAGED_CONFIG_H_

/* Genode includes */
#include <os/reporter.h>
#include <base/attached_rom_dataspace.h>

/* local includes */
#include <types.h>

namespace Sculpt { template <typename> struct Managed_config; }


template <typename HANDLER>
struct Sculpt::Managed_config
{
	Env &_env;

	Allocator &_alloc;

	using Rom_name = String<32>;
	using Label    = Session_label;

	bool managed = true;

	HANDLER &_obj;

	void (HANDLER::*_fn) (Node const &);

	/*
	 * Imported configuration
	 */
	Attached_rom_dataspace _rom;

	Constructible<Buffered_node> _buffered { };

	/*
	 * Exported configuration
	 */
	Expanding_reporter _report;

	Signal_handler<Managed_config> _handler {
		_env.ep(), *this, &Managed_config::_handle };

	void _handle()
	{
		_rom.update();

		Node const &config = _rom.node();

		managed = config.has_type("empty")
		       || config.attribute_value("managed", false);

		bool const same = _buffered.constructed() && !_buffered->differs_from(config);
		if (same)
			return;

		_buffered.construct(_alloc, config);

		(_obj.*_fn)(config);
	}

	void with_node(auto const &fn) const
	{
		fn(_rom.node());
	}

	void generate(auto const &fn)
	{
		_report.generate([&] (Generator &g) {
			if (managed)
				g.attribute("managed", "yes");
			fn(g);
		});
	}

	Managed_config(Env &env, Allocator &alloc, Node::Type const &node_type,
	               Rom_name const &rom_name,
	               HANDLER &obj, void (HANDLER::*fn) (Node const &))
	:
		_env(env), _alloc(alloc), _obj(obj), _fn(fn),
		_rom(_env, Label("model -> ", rom_name).string()),
		_report(_env, node_type.string(), Label(rom_name, "_config").string())
	{
		_rom.sigh(_handler);
		trigger_update();
	}

	void trigger_update()
	{
		_buffered.destruct();
		_handler.local_submit();
	}
};

#endif /* _MANAGED_CONFIG_H_ */

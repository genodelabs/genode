/*
 * \brief  Sculpt MMC-driver management
 * \author Norman Feske
 * \date   2024-03-25
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__MMC_H_
#define _DRIVER__MMC_H_

namespace Sculpt { struct Mmc_driver; }


struct Sculpt::Mmc_driver : private Noncopyable
{
	struct Action : Interface
	{
		virtual void handle_mmc_discovered() = 0;
	};

	Env    &_env;
	Action &_action;

	Constructible<Child_state> _mmc { };

	Rom_handler<Mmc_driver> _devices {
		_env, "report -> runtime/mmc/block_devices", *this, &Mmc_driver::_handle_devices };

	void _handle_devices(Node const &) { _action.handle_mmc_discovered(); }

	Mmc_driver(Env &env, Action &action) : _env(env), _action(action) { }

	void gen_start_node(Generator &g) const
	{
		if (!_mmc.constructed())
			return;

		g.node("start", [&] {
			_mmc->gen_start_node_content(g);
			gen_named_node(g, "binary", "mmc");
			gen_provides<Block::Session>(g);
			g.node("config", [&] {
				g.attribute("report", "yes");
				for (unsigned i = 0; i < 4; i++) {
					String<64> name("mmcblk", i);
					g.node("policy", [&] {
						g.attribute("label", name);
						g.attribute("device", name);
						g.attribute("writeable", "yes");
					});
				}
			});
			g.node("route", [&] {
				gen_parent_route<Platform::Session>(g);
				gen_parent_rom_route(g, "dtb", "mmc.dtb");
				gen_parent_rom_route(g, "mmc");
				gen_common_routes(g);
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		_mmc.conditional(board_info.soc.mmc,
		                 registry, "mmc", Priority::DEFAULT,
		                 Ram_quota { 16*1024*1024 }, Cap_quota { 500 });
	}

	void with_devices(auto const &fn) const
	{
		_devices.with_node([&] (Node const &devices) {
			fn({ .present = _mmc.constructed(), .report = devices }); });
	}
};

#endif /* _DRIVER__MMC_H_ */

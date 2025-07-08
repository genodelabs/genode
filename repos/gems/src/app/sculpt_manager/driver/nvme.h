/*
 * \brief  Sculpt NVME-driver management
 * \author Norman Feske
 * \date   2024-03-21
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER_NVME_H_
#define _DRIVER_NVME_H_

namespace Sculpt { struct Nvme_driver; }


struct Sculpt::Nvme_driver : private Noncopyable
{
	struct Action : Interface
	{
		virtual void handle_nvme_discovered() = 0;
	};

	Env    &_env;
	Action &_action;

	Constructible<Child_state> _nvme { };

	Rom_handler<Nvme_driver> _namespaces {
		_env, "report -> runtime/nvme/controller", *this, &Nvme_driver::_handle_namespaces };

	void _handle_namespaces(Node const &) { _action.handle_nvme_discovered(); }

	Nvme_driver(Env &env, Action &action) : _env(env), _action(action) { }

	void gen_start_node(Generator &g) const
	{
		if (!_nvme.constructed())
			return;

		g.node("start", [&] {
			_nvme->gen_start_node_content(g);
			gen_named_node(g, "binary", "nvme");
			gen_provides<Block::Session>(g);
			g.node("config", [&] {
				g.attribute("system", "yes");
				g.node("report", [&] { g.attribute("namespaces", "yes"); });
				g.node("policy", [&] {
					g.attribute("label",     1);
					g.attribute("namespace", 1);
					g.attribute("writeable", "yes"); });
			});
			g.node("route", [&] {
				gen_parent_route<Platform::Session>(g);
				gen_parent_rom_route(g, "nvme");
				gen_parent_rom_route(g, "system",   "config -> managed/system");
				gen_common_routes(g);
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		_nvme.conditional(board_info.detected.nvme,
		                  registry, "nvme", Priority::DEFAULT,
		                  Ram_quota { 8*1024*1024 }, Cap_quota { 100 });
	}

	void with_namespaces(auto const &fn) const
	{
		_namespaces.with_node([&] (Node const &namespaces) {
			fn({ .present = _nvme.constructed(), .report = namespaces }); });
	}
};

#endif /* _DRIVER_NVME_H_ */

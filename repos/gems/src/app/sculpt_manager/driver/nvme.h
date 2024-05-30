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

	void _handle_namespaces(Xml_node const &) { _action.handle_nvme_discovered(); }

	Nvme_driver(Env &env, Action &action) : _env(env), _action(action) { }

	void gen_start_node(Xml_generator &xml) const
	{
		if (!_nvme.constructed())
			return;

		xml.node("start", [&] {
			_nvme->gen_start_node_content(xml);
			gen_named_node(xml, "binary", "nvme");
			gen_provides<Block::Session>(xml);
			xml.node("config", [&] {
				xml.attribute("system", "yes");
				xml.node("report", [&] { xml.attribute("namespaces", "yes"); });
				xml.node("policy", [&] {
					xml.attribute("label",     1);
					xml.attribute("namespace", 1);
					xml.attribute("writeable", "yes"); });
			});
			xml.node("route", [&] {
				gen_parent_route<Platform::Session>(xml);
				gen_parent_rom_route(xml, "nvme");
				gen_parent_rom_route(xml, "system",   "config -> managed/system");
				gen_common_routes(xml);
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
		_namespaces.with_xml([&] (Xml_node const &namespaces) {
			fn(_nvme.constructed() ? namespaces : Xml_node("<none/>")); });
	}
};

#endif /* _DRIVER_NVME_H_ */

/*
 * \brief  Utility for querying the directory structure of file systems
 * \author Norman Feske
 * \date   2025-04-17
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__DIR_QUERY_H_
#define _MODEL__DIR_QUERY_H_

#include <types.h>
#include <managed_config.h>
#include <model/child_state.h>

namespace Sculpt { struct Dir_query; }

struct Sculpt::Dir_query : Noncopyable
{
	using Identity = Start_name;  /* 'component name' -> 'fs session label' */
	using Fs_name  = String<128>;

	struct Action : Interface
	{
		virtual void queried_dir_response() = 0;
	};

	Action &_action;

	struct Query
	{
		Identity identity;  /* label of designated file-system client */
		Fs_name  fs;        /* queried fs, or "" for all file systems */
		Path     path;      /* fs-local directory */

		Path vfs_path() const { return (fs == "") ? Path { "/" }
		                                          : Path { "/", fs, "/", path }; }

		bool operator != (Query const &other) const
		{
			return other.fs != fs || other.path != path || other.identity != identity;
		}
	};

	Query _query { };

	/*
	 * Dictionary of known file systems
	 */
	struct Fs;
	using Fs_dict = Dictionary<Fs, Fs_name>;
	struct Fs : Fs_dict::Element
	{
		bool const parent;

		Fs(Fs_dict &dict, Fs_name const &fs_name, bool parent)
		: Fs_dict::Element(dict, fs_name), parent(parent) { }
	};
	Fs_dict _fs_dict { };

	struct State : Noncopyable
	{
		Action &_action;

		Child_state _fs_query;

		Rom_handler<State> _listing_handler;

		void _handle_listing(Xml_node const &) { _action.queried_dir_response(); }

		State(Env &env, Action &action, Registry<Child_state> &children)
		:
			_action(action),
			_fs_query(children, {
				.name      = "dir_query",
				.priority  = Priority::STORAGE,
				.cpu_quota = { },
				.location  = { },
				.initial   = { .ram =  4*1024*1024, .caps = 1000 },
				.max       = { .ram = 16*1024*1024, .caps = 2000 } }),
			_listing_handler(env, { "report -> runtime/dir_query/listing" },
			                 *this, &State::_handle_listing)
		{ }
	};

	Constructible<State> _state { };

	Managed_config<Dir_query> _fs_query_config;

	void _gen_fs_query_config()
	{
		_fs_query_config.generate([&] (Xml_generator &xml) {
			xml.node("vfs", [&] {
				_fs_dict.for_each([&] (Fs const &fs) {
					if (_query.fs == "" || _query.fs == fs.name)
						gen_named_node(xml, "dir", fs.name, [&] {
							xml.node("fs", [&] {
								xml.attribute("label", fs.name); }); }); }); });

			xml.node("query", [&] {
				xml.attribute("path", _query.vfs_path());
				xml.attribute("count", "yes"); });
		});
	}

	void _handle_fs_query_config(Xml_node const &)
	{
		_gen_fs_query_config();
	}

	Dir_query(Env &env, Action &action)
	:
		_action(action),
		_fs_query_config(env, "config", "dir_query",
		                 *this, &Dir_query::_handle_fs_query_config)
	{
		_fs_query_config.trigger_update();
	}

	struct [[nodiscard]] Update_result { bool runtime_reconfig_needed; };

	/**
	 * Respond to appearing/disappearing file systems
	 */
	Update_result update(Allocator &alloc, Runtime_config const &runtime_config)
	{
		bool any_file_system_vanished = false;
		_fs_dict.for_each([&] (Fs const &fs) {
			bool exists = false;
			runtime_config.for_each_service([&] (Service const &service) {
				exists |= (service.type == Service::Type::FILE_SYSTEM)
				       && (service.fs_name() == fs.name); });
			if (!exists) {
				any_file_system_vanished = true;
			}
		});

		if (any_file_system_vanished)
			while (_fs_dict.with_any_element([&] (Fs &fs) {
				destroy(alloc, &fs); }));

		bool file_systems_changed = any_file_system_vanished;
		runtime_config.for_each_service([&] (Service const &service) {
			if (service.type == Service::Type::FILE_SYSTEM)
				if (!_fs_dict.exists(service.fs_name())) {
					new (alloc) Fs(_fs_dict, service.fs_name(), !service.server.valid());
					file_systems_changed = true; } });

		if (file_systems_changed) {
			_gen_fs_query_config();
			if (_state.constructed())
				_state->_fs_query.trigger_restart();
		}

		return { .runtime_reconfig_needed = file_systems_changed };
	}

	Update_result update_query(Env &env, Action &action,
	                           Registry<Child_state> &children, Query const &query)
	{
		Query const orig_query = _query;
		_query = query;

		_gen_fs_query_config();

		if (!_state.constructed())
			_state.construct(env, action, children);

		bool const vfs_needs_reconstruct = (orig_query.fs       != _query.fs)
		                                || (orig_query.identity != _query.identity);
		if (vfs_needs_reconstruct)
			_state->_fs_query.trigger_restart();

		return { .runtime_reconfig_needed = vfs_needs_reconstruct };
	}

	Update_result drop_query()
	{
		Update_result result { .runtime_reconfig_needed = _state.constructed() };
		_state.destruct();
		_query = { };
		return result;
	}

	struct Entry
	{
		using Name = String<128>;

		unsigned index;
		Name     name;
		unsigned num_dirs;
	};

	void for_each_dir_entry(Query const &query, auto const &fn) const
	{
		if (query != _query || !_state.constructed())
			return;

		_state->_listing_handler.with_xml([&] (Xml_node const &listing) {
			unsigned index = 0;
			listing.for_each_sub_node("dir", [&] (Xml_node const &dir_response) {
				if (dir_response.attribute_value("path", Path()) != query.vfs_path())
					return;

				dir_response.for_each_sub_node("dir", [&] (Xml_node const &dir) {
					fn(Entry { .index = index++,
					           .name = dir.attribute_value("name", Entry::Name()),
					           .num_dirs = dir.attribute_value("num_dirs", 0u) });
				});
			});
		});
	}

	bool dir_entry_has_sub_dirs(Query const &query, String<128> const &sub_dir) const
	{
		bool result = false;
		for_each_dir_entry(query, [&] (Entry const &entry) {
			if (entry.name == sub_dir && entry.num_dirs)
				result = true; });
		return result;
	}

	void gen_start_nodes(Xml_generator &xml) const
	{
		if (!_state.constructed())
			return;

		auto gen_fs_route = [&] (Xml_generator &xml, Fs const &fs)
		{
			gen_service_node<::File_system::Session>(xml, [&] {
				xml.attribute("label", fs.name);
				if (fs.parent)
					xml.node("parent", [&] {
						xml.attribute("identity", fs.name);
						xml.attribute("resource", "/"); });
				else
					xml.node("child", [&] {
						xml.attribute("name",     fs.name);
						xml.attribute("identity", _query.identity);
						xml.attribute("resource", "/"); });
			});
		};

		xml.node("start", [&] {
			_state->_fs_query.gen_start_node_content(xml);

			gen_named_node(xml, "binary", "fs_query");

			xml.node("route", [&] {
				gen_parent_rom_route(xml, "fs_query");
				gen_parent_rom_route(xml, "config", "config -> managed/dir_query");
				gen_parent_rom_route(xml, "ld.lib.so");
				gen_parent_rom_route(xml, "vfs.lib.so");

				gen_parent_route<Cpu_session>     (xml);
				gen_parent_route<Pd_session>      (xml);
				gen_parent_route<Log_session>     (xml);
				gen_parent_route<Report::Session> (xml);

				_fs_dict.for_each([&] (Fs const &fs) {
					if (_query.fs == "" || _query.fs == fs.name)
						gen_fs_route(xml, fs); });
			});
		});
	}
};

#endif /* _MODEL__DIR_QUERY_H_ */

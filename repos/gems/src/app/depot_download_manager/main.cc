/*
 * \brief  Tool for managing the download of depot content
 * \author Norman Feske
 * \date   2017-12-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* local includes */
#include "xml.h"

namespace Depot_download_manager {
	using namespace Depot;
	struct Child_exit_state;
	struct Main;
}


struct Depot_download_manager::Child_exit_state
{
	bool exists = false;
	bool exited = false;
	int  code   = 0;

	typedef String<64> Name;

	Child_exit_state(Xml_node init_state, Name const &name)
	{
		init_state.for_each_sub_node("child", [&] (Xml_node child) {
			if (child.attribute_value("name", Name()) == name) {
				exists = true;
				if (child.has_attribute("exited")) {
					exited = true;
					code = child.attribute_value("exited", 0L);
				}
			}
		});
	}
};


struct Depot_download_manager::Main : Import::Download_progress
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _installation      { _env, "installation"      };
	Attached_rom_dataspace _dependencies      { _env, "dependencies"      };
	Attached_rom_dataspace _init_state        { _env, "init_state"        };
	Attached_rom_dataspace _fetchurl_progress { _env, "fetchurl_progress" };

	/**
	 * User identity, from which current downloads are fetched
	 */
	Attached_rom_dataspace _current_user { _env, "user" };

	/**
	 * Result of signature verification, reported by the 'verify' component
	 */
	Attached_rom_dataspace _verified { _env, "verified" };

	class Invalid_download_url : Exception { };

	/**
	 * \throw Invalid_download_url
	 */
	Url _current_user_url() const;

	Archive::User _current_user_name() const
	{
		return _current_user.xml().attribute_value("name", Archive::User());
	}

	Path _current_user_path() const
	{
		return Path("/depot/", _current_user_name());
	}

	Expanding_reporter _init_config { _env, "config", "init_config" };

	Expanding_reporter _state_reporter { _env, "state", "state" };

	/**
	 * Version counters, used to enforce the restart or reconfiguration of
	 * components.
	 */
	Depot_query_version _depot_query_count { 1 };
	Fetchurl_version    _fetchurl_count    { 1 };

	unsigned const _fetchurl_max_attempts = 3;
	unsigned       _fetchurl_attempt      = 0;

	Archive::User _next_user { };

	Constructible<Import> _import { };

	/**
	 * Download_progress interface
	 */
	Info download_progress(Archive::Path const &path) const
	{
		Info result { Info::Bytes(), Info::Bytes() };
		try {
			Url const url_path(_current_user_url(), "/", path, ".tar.xz");

			/* search fetchurl progress report for matching 'url_path' */
			_fetchurl_progress.xml().for_each_sub_node("fetch", [&] (Xml_node fetch) {
				if (fetch.attribute_value("url", Url()) == url_path)
					result = { .total = fetch.attribute_value("total", Info::Bytes()),
					           .now   = fetch.attribute_value("now",   Info::Bytes()) }; });

		} catch (Invalid_download_url) { }
		return result;
	}

	void _update_state_report()
	{
		_state_reporter.generate([&] (Xml_generator &xml) {
			if (_import.constructed())
				_import->report(xml, *this); });
	}

	void _generate_init_config(Xml_generator &);

	void _generate_init_config()
	{
		_init_config.generate([&] (Xml_generator &xml) {
			_generate_init_config(xml); });
	}

	void _handle_installation()
	{
		_installation.update();
		_generate_init_config();
	}

	Signal_handler<Main> _installation_handler {
		_env.ep(), *this, &Main::_handle_installation };

	Signal_handler<Main> _query_result_handler {
		_env.ep(), *this, &Main::_handle_query_result };

	void _handle_query_result();

	Signal_handler<Main> _init_state_handler {
		_env.ep(), *this, &Main::_handle_init_state };

	void _handle_init_state();

	Signal_handler<Main> _fetchurl_progress_handler {
		_env.ep(), *this, &Main::_handle_fetchurl_progress };

	void _handle_fetchurl_progress()
	{
		_fetchurl_progress.update();
		_update_state_report();
	}

	Main(Env &env) : _env(env)
	{
		_dependencies     .sigh(_query_result_handler);
		_current_user     .sigh(_query_result_handler);
		_init_state       .sigh(_init_state_handler);
		_verified         .sigh(_init_state_handler);
		_installation     .sigh(_installation_handler);
		_fetchurl_progress.sigh(_fetchurl_progress_handler);

		_generate_init_config();
	}
};


Depot_download_manager::Url
Depot_download_manager::Main::_current_user_url() const
{
	if (!_current_user.xml().has_sub_node("url"))
		throw Invalid_download_url();

	Url const url = _current_user.xml().sub_node("url").decoded_content<Url>();

	/*
	 * Ensure that the URL does not contain any '"' character because it will
	 * be taken as an XML attribute value.
	 *
	 * XXX This should better be addressed by sanitizing the argument of
	 *     'Xml_generator::attribute' (issue #1757).
	 */
	for (char const *s = url.string(); *s; s++)
		if (*s == '"')
			throw Invalid_download_url();

	return url;
}


void Depot_download_manager::Main::_generate_init_config(Xml_generator &xml)
{
	xml.node("report", [&] () {
		xml.attribute("delay_ms", 500); });

	xml.node("parent-provides", [&] () {
		gen_parent_service<Rom_session>(xml);
		gen_parent_service<Cpu_session>(xml);
		gen_parent_service<Pd_session>(xml);
		gen_parent_service<Log_session>(xml);
		gen_parent_service<Timer::Session>(xml);
		gen_parent_service<Report::Session>(xml);
		gen_parent_service<Nic::Session>(xml);
		gen_parent_service<File_system::Session>(xml);
	});

	xml.node("start", [&] () {
		gen_depot_query_start_content(xml, _installation.xml(),
		                              _next_user, _depot_query_count); });

	if (_import.constructed() && _import->downloads_in_progress()) {
		try {
			xml.node("start", [&] () {
				gen_fetchurl_start_content(xml, *_import, _current_user_url(),
				                           _fetchurl_count); });
		}
		catch (Invalid_download_url) {
			error("invalid download URL for depot user:", _current_user.xml());
		}
	}

	if (_import.constructed() && _import->unverified_archives_available())
		xml.node("start", [&] () {
			gen_verify_start_content(xml, *_import, _current_user_path()); });

	if (_import.constructed() && _import->verified_archives_available()) {

		xml.node("start", [&] () {
			gen_chroot_start_content(xml, _current_user_name());  });

		xml.node("start", [&] () {
			gen_extract_start_content(xml, *_import, _current_user_path(),
			                          _current_user_name()); });
	}
}


void Depot_download_manager::Main::_handle_query_result()
{
	/* finish current import before starting a new one */
	if (_import.constructed())
		return;

	_dependencies.update();
	_current_user.update();

	Xml_node const dependencies = _dependencies.xml();
	if (dependencies.num_sub_nodes() == 0)
		return;

	if (!dependencies.has_sub_node("missing")) {
		log("installation complete.");
		_update_state_report();
		return;
	}

	Archive::Path const path =
		dependencies.sub_node("missing").attribute_value("path", Archive::Path());

	if (Archive::user(path) != _current_user_name()) {
		_next_user = Archive::user(path);

		/* query user info from 'depot_query' */
		_generate_init_config();
		return;
	}

	/* start new import */
	_import.construct(_heap, _current_user_name(), _dependencies.xml());
	_fetchurl_attempt = 0;
	_update_state_report();

	/* spawn fetchurl */
	_generate_init_config();
}


void Depot_download_manager::Main::_handle_init_state()
{
	_init_state.update();
	_verified.update();

	bool reconfigure_init = false;
	bool import_finished  = false;

	if (!_import.constructed())
		return;

	Import &import = *_import;

	if (import.downloads_in_progress()) {

		Child_exit_state const fetchurl_state(_init_state.xml(), "fetchurl");

		if (fetchurl_state.exited && fetchurl_state.code != 0) {
			error("fetchurl failed with exit code ", fetchurl_state.code);

			/* retry by incrementing the version attribute of the start node */
			_fetchurl_count.value++;

			if (_fetchurl_attempt++ >= _fetchurl_max_attempts) {
				import.all_downloads_unavailable();
				_fetchurl_attempt = 0;
			}

			reconfigure_init = true;
		}

		if (fetchurl_state.exited && fetchurl_state.code == 0) {
			import.all_downloads_completed();

			/* kill fetchurl, start untar */
			reconfigure_init = true;
		}
	}

	if (import.unverified_archives_available()) {

		_verified.xml().for_each_sub_node([&] (Xml_node node) {

			/* path in the VFS name space of the 'verify' component */
			Path const abs_path = node.attribute_value("path", Archive::Path());

			/* determine matching archive path */
			Path path;
			import.for_each_unverified_archive([&] (Archive::Path const &archive) {
				if (abs_path == Path("/public/", archive, ".tar.xz"))
					path = archive; });

			if (path.valid()) {
				if (node.type() == "good") {
					import.archive_verified(path);
				} else {
					error("signature check failed for '", path, "' "
					      "(", node.attribute_value("reason", String<64>()), ")");
					import.archive_verification_failed(path);
				}
			}

			reconfigure_init = true;
		});
	}

	if (import.verified_archives_available()) {

		Child_exit_state const extract_state(_init_state.xml(), "extract");

		if (extract_state.exited && extract_state.code != 0)
			error("extract failed with exit code ", extract_state.code);

		if (extract_state.exited && extract_state.code == 0) {
			import.all_verified_archives_extracted();
			import_finished = true;

			/* kill extract, re-issue new depot query to start next iteration */
			_depot_query_count.value++;
			reconfigure_init = true;
		}
	}

	/* report before destructing '_import' to avoid empty intermediate reports */
	if (reconfigure_init)
		_update_state_report();

	if (import_finished)
		_import.destruct();

	if (reconfigure_init)
		_generate_init_config();
}


void Component::construct(Genode::Env &env)
{
	static Depot_download_manager::Main main(env);
}


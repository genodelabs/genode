/*
 * \brief  Tool for deleting packages from a depot and resolving unused dependencies
 * \author Alice Domage
 * \date   2023-06-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 * Copyright (C) 2023 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/registry.h>
#include <base/signal.h>
#include <depot/archive.h>
#include <os/reporter.h>
#include <os/vfs.h>
#include <util/reconstructible.h>
#include <util/xml_node.h>
#include <util/xml_generator.h>


namespace Depot_remove {

	using namespace Genode;

	struct Main;
	class  Archive_remover;

}


class Depot_remove::Archive_remover
{
	public:

		using Archive_path    = Depot::Archive::Path;
		using Registered_path = Genode::Registered_no_delete<Archive_path>;
		using Path            = Directory::Path;


	private:

		Allocator                &_alloc;
		String<32> const          _arch;
		Registry<Registered_path> _deleted_archives  { };
		Registry<Registered_path> _pkg_to_delete     { };
		Registry<Registered_path> _archive_to_delete { };

		void _remove_directory(Directory &depot, Path path) const
		{
			Directory                 dir { depot, path };
			Registry<Registered_path> dirent_files { };

			dir.for_each_entry([&] (auto const &entry) {
				if (entry.name() == ".." || entry.name() == ".")
					return;
				else if (entry.type() == Vfs::Directory_service::Dirent_type::DIRECTORY)
					_remove_directory(depot, Directory::join(path, entry.name()));
				else
					/*
					 * Deleting file within the for_each_entry() confuses lx_fs dirent
					 * offset computation and some files, such as 'README', is consitently
					 * omitted, thus the unlink operation fails. Thus create a list
					 * to delete file out of the lambda.
					 */
					new (_alloc) Registered_path(dirent_files, Directory::join(path, entry.name())); });

			dirent_files.for_each([&](Registered_path &sub_path) {
				depot.unlink(sub_path);
				destroy(_alloc, &sub_path); });

			depot.unlink(path);
		}

		template<typename FUNC = void(Path const &)>
		void _for_each_subdir(Directory &depot, Path const &parent_dir, FUNC fn)
		{
			Directory pkg { depot, parent_dir };

			pkg.for_each_entry([&fn, &parent_dir](auto const &entry) {
				if (entry.name() == ".." || entry.name() == ".")
					return;
				Path subdir_path { Directory::join(parent_dir, entry.name()) };
				fn(subdir_path); });
		}

		template<typename FUNC = void(Path const &)>
		void _for_each_pkg(Directory &depot, FUNC fn)
		{
			depot.for_each_entry([&](auto const &entry) {
				Path pkg_path { entry.name(), "/pkg" };
				if (depot.directory_exists(pkg_path)) {
					_for_each_subdir(depot, pkg_path, [&] (Path const &pkg_path) {
						_for_each_subdir(depot, pkg_path, [&] (Path const &pkg_version_path) {
							fn(pkg_version_path); }); }); } });
		}

		void _autoremove_pkg_and_dependencies(Directory &depot)
		{

			/* collect all archive dependencies to delete */
			_pkg_to_delete.for_each([&](Archive_path &elem) {
				Path pkg_version_path { elem };
				Path archive_file_path { Directory::join(pkg_version_path, "archives") };
				File_content archives { _alloc, depot, archive_file_path, { 8192 } };
				archives.for_each_line<Path>([&](auto const &dependency_path) {
					if (Depot::Archive::type(dependency_path) == Depot::Archive::Type::PKG)
						return;
					new (_alloc) Registered_path(_archive_to_delete, dependency_path); });
				_remove_directory(depot, pkg_version_path);
				/* try to delete the parent if it is empty, if not empty the operation fails */
				_remove_directory(depot, Genode::Directory::join(pkg_version_path, ".."));
				new (_alloc) Registered_path(_deleted_archives, pkg_version_path); });

			/* keep archive dependencies that are still referenced by another PKG */
			_for_each_pkg(depot, [&](Path const &pkg_version_path) {
				Path archive_file_path { Directory::join(pkg_version_path, "archives") };
				File_content archives  { _alloc, depot, archive_file_path, { 8192 } };
				archives.for_each_line<Path>([&] (auto const &dependency_path) {
					if (Depot::Archive::type(dependency_path) == Depot::Archive::Type::PKG)
						return;
					_archive_to_delete.for_each([&](Registered_path &path){
						if (dependency_path == path) 
							destroy(_alloc, &path); }); }); });

			/* delete archive dependencies */
			_archive_to_delete.for_each([&](Archive_path &path) {
				Path archive {};
				if (Depot::Archive::type(path) == Depot::Archive::Type::SRC) {
					archive = Directory::join(Depot::Archive::user(path), "bin");
					archive = Directory::join(archive, _arch);
					archive = Directory::join(archive, Depot::Archive::name(path));
					archive = Directory::join(archive, Depot::Archive::version(path));
				} else {
					archive = path;
				}
				/* if directory does not exist, it might has been deleted before, return silently */
				if (!depot.directory_exists(archive))
					return;
				_remove_directory(depot, archive);
				new (_alloc) Registered_path(_deleted_archives, archive);
				/* try to delete the parent if it is empty, if not empty the operation fails */
				_remove_directory(depot, Directory::join(archive, "..")); });
		}

		static bool _config_node_match_pkg(Genode::Xml_node const &node, Path pkg)
		{
			if (!node.has_attribute("user"))
				return false;

			if (Depot::Archive::user(pkg) != node.attribute_value("user", Archive_path {}))
				return false;

			if (!node.has_attribute("pkg"))
				return true;

			if (Depot::Archive::name(pkg) != node.attribute_value("pkg", Archive_path {}))
				return false;

			if (!node.has_attribute("version"))
				return true;

			if (Depot::Archive::version(pkg) != node.attribute_value("version", Archive_path {}))
				return false;

			return true;
		};

		void _configure_remove_pkgs(Directory &depot, Xml_node const &config)
		{
			_for_each_pkg(depot, [&] (Path const &pkg_path) {
				config.for_each_sub_node("remove", [&](Xml_node const &node) {
					if (_config_node_match_pkg(node, pkg_path))
						new (_alloc) Registered_path(_pkg_to_delete, pkg_path); }); });
		}

		void _configure_remove_all_pkgs(Directory &depot, Xml_node const &config)
		{
			_for_each_pkg(depot, [&] (Path const &pkg_path) {
				bool keep = false;
				config.for_each_sub_node("remove-all", [&](Xml_node const &remove_all_node) {
					remove_all_node.for_each_sub_node("keep", [&](Xml_node const &node) {
						if (_config_node_match_pkg(node, pkg_path))
							keep = true; }); });
				if (!keep)
					new (_alloc) Registered_path(_pkg_to_delete, pkg_path); });
		}


	public:

		void generate_report(Expanding_reporter &reporter) const
		{
			reporter.generate([&](Reporter::Xml_generator &xml) {
				_deleted_archives.for_each([&] (auto &path) {
					xml.node("removed", [&]() {
						xml.attribute("path", path); }); }); });
		}

		Archive_remover(Allocator      &alloc,
		                Directory      &depot,
		                Xml_node const &config)
		:
			_alloc { alloc },
			_arch  { config.attribute_value("arch", String<32>()) }
		{
			if (config.has_sub_node("remove") && config.has_sub_node("remove-all")) {
				warning("<remove/> and <remove-all/> are mutually exclusive");
				return;
			}

			if (config.has_sub_node("remove")) _configure_remove_pkgs(depot, config);
			if (config.has_sub_node("remove-all")) _configure_remove_all_pkgs(depot, config);

			_autoremove_pkg_and_dependencies(depot);
		}

		~Archive_remover() {
			_pkg_to_delete.for_each([this] (auto &elem) {
				destroy(_alloc, &elem); });
			_archive_to_delete.for_each([this] (auto &elem) {
				destroy(_alloc, &elem); });
			_deleted_archives.for_each([this] (auto &elem) {
				destroy(_alloc, &elem); });
		}
};


struct Depot_remove::Main
{
	Env                              &_env;
	Heap                              _heap           { _env.ram(), _env.rm() };
	Attached_rom_dataspace            _config_rom     { _env, "config" };
	Signal_handler<Main>              _config_handler { _env.ep(), *this, &Main::_handle_config };
	Constructible<Expanding_reporter> _reporter       { };

	Main(Env &env)
	:
		_env { env },
		_config_handler { env.ep(), *this, &Main::_handle_config }
	{
		_config_rom.sigh(_config_handler);
		_handle_config();
	}

	void _handle_config()
	{
		_config_rom.update();
		Xml_node const &config { _config_rom.xml() };

		if (!config.has_attribute("arch")) {
			warning("missing arch attribute");
			return;
		}

		if (!config.has_sub_node("vfs")) {
			warning("configuration misses a <vfs> configuration node");
			return;
		}

		Directory::Path depot_path     { "depot" };
		Root_directory  root_directory { _env, _heap, config.sub_node("vfs") };
		Directory       depot          { root_directory, depot_path };

		try {
			Archive_remover archive_cleaner { _heap, depot, config };

			_reporter.conditional(config.attribute_value("report", false),
			                      _env, "removed_archives", "archive_list");

			if (_reporter.constructed())
				archive_cleaner.generate_report(*_reporter);
		} catch (...) {
			/* catch any exceptions to prevent the component to abort */
			error("Depot autoclean job finished with error(s).");
		}
	}
};


void Component::construct(Genode::Env &env)
{
	static Depot_remove::Main main(env);
}


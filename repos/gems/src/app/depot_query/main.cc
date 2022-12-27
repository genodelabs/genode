/*
 * \brief  Tool for querying subsystem information from a depot
 * \author Norman Feske
 * \date   2017-07-04
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
#include <os/vfs.h>
#include <util/dictionary.h>
#include <depot/archive.h>

/* fs_query includes */
#include <for_each_subdir_name.h>

namespace Depot_query {

	using namespace Depot;

	typedef String<64> Rom_label;

	struct Directory_cache;
	struct Recursion_limit;
	struct Dependencies;
	struct Main;
}


struct Depot_query::Directory_cache : Noncopyable
{
	Allocator &_alloc;

	using Name = Directory::Entry::Name;

	struct Listing;
	using Listings = Dictionary<Listing, Directory::Path>;

	struct Listing : Listings::Element
	{
		Allocator &_alloc;

		struct File;
		using Files = Dictionary<File, Name>;

		struct File : Files::Element
		{
			File(Files &files, Name const &name) : Files::Element(files, name) { }
		};

		Files _files { };

		Listing(Listings &listings, Allocator &alloc, Directory &dir,
		        Directory::Path const &path)
		:
			Listings::Element(listings, path), _alloc(alloc)
		{
			try {
				Directory(dir, path).for_each_entry([&] (Directory::Entry const &entry) {
					new (_alloc) File(_files, entry.name()); });
			}
			catch (Directory::Nonexistent_directory) {
				warning("directory '", path, "' does not exist");
			}
		}

		~Listing()
		{
			auto destroy_fn = [&] (File &f) { destroy(_alloc, &f); };

			while (_files.with_any_element(destroy_fn));
		}

		bool file_exists(Name const &name) const { return _files.exists(name); }
	};

	Listings mutable _listings { };

	Directory_cache(Allocator &alloc) : _alloc(alloc) { }

	~Directory_cache()
	{
		auto destroy_fn = [&] (Listing &l) { destroy(_alloc, &l); };

		while (_listings.with_any_element(destroy_fn));
	}

	bool file_exists(Directory &dir, Directory::Path const &path, Name const &name) const
	{
		bool listing_known = false;

		bool const result =
			_listings.with_element(path,
				[&] /* match */ (Listing const &listing) {
					listing_known = true;
					return listing.file_exists(name);
				},
				[&] /* no_match */ { return false; });

		if (listing_known)
			return result;

		Listing &new_listing = *new (_alloc) Listing(_listings, _alloc, dir, path);

		return new_listing.file_exists(name);
	}
};


class Depot_query::Recursion_limit : Noncopyable
{
	public:

		class Reached : Exception { };

	private:

		unsigned const _value;

		static unsigned _checked_decr(unsigned value)
		{
			if (value == 0)
				throw Reached();

			return value - 1;
		}

	public:

		/**
		 * Constructor
		 */
		explicit Recursion_limit(unsigned value) : _value(value) { }

		/**
		 * Copy constructor
		 *
		 * \throw Recursion_limit::Reached
		 */
		Recursion_limit(Recursion_limit const &other)
		: Noncopyable(), _value(_checked_decr(other._value)) { }
};


/**
 * Collection of dependencies
 *
 * This data structure keeps track of a list of archive paths along with the
 * information of whether or not the archive is present in the depot. It also
 * ensures that all entries are unique.
 */
class Depot_query::Dependencies
{
	private:

		struct Collection : Noncopyable
		{
			Allocator &_alloc;

			typedef Registered_no_delete<Archive::Path> Entry;

			Registry<Entry> _entries { };

			Collection(Allocator &alloc) : _alloc(alloc) { }

			~Collection()
			{
				_entries.for_each([&] (Entry &e) { destroy(_alloc, &e); });
			}

			bool known(Archive::Path const &path) const
			{
				bool result = false;
				_entries.for_each([&] (Entry const &entry) {
					if (path == entry)
						result = true; });

				return result;
			}

			void insert(Archive::Path const &path)
			{
				if (!known(path))
					new (_alloc) Entry(_entries, path);
			}

			template <typename FN>
			void for_each(FN const &fn) const { _entries.for_each(fn); };
		};

		Directory const &_depot;

		Collection _present;
		Collection _missing;

	public:

		Dependencies(Allocator &alloc, Directory const &depot)
		:
			_depot(depot), _present(alloc), _missing(alloc)
		{ }

		bool known(Archive::Path const &path) const
		{
			return _present.known(path) || _missing.known(path);
		}

		void record(Archive::Path const &path)
		{
			if (_depot.directory_exists(path))
				_present.insert(path);
			else
				_missing.insert(path);
		}

		void xml(Xml_generator &xml) const
		{
			_present.for_each([&] (Archive::Path const &path) {
				xml.node("present", [&] () { xml.attribute("path", path); }); });

			_missing.for_each([&] (Archive::Path const &path) {
				xml.node("missing", [&] () { xml.attribute("path", path); }); });
		}
};


struct Depot_query::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Constructible<Attached_rom_dataspace> _query_rom { };

	Root_directory _root { _env, _heap, _config.xml().sub_node("vfs") };

	Directory _depot_dir { _root, "depot" };

	Constructible<Directory_cache> _directory_cache { };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Signal_handler<Main> _query_handler {
		_env.ep(), *this, &Main::_handle_config };

	typedef Constructible<Expanding_reporter> Constructible_reporter;

	Constructible_reporter _scan_reporter         { };
	Constructible_reporter _blueprint_reporter    { };
	Constructible_reporter _dependencies_reporter { };
	Constructible_reporter _user_reporter         { };
	Constructible_reporter _index_reporter        { };

	template <typename T, typename... ARGS>
	static void _construct_if(bool condition, Constructible<T> &obj, ARGS &&... args)
	{
		if (condition && !obj.constructed())
			obj.construct(args...);

		if (!condition && obj.constructed())
			obj.destruct();
	}

	typedef String<16> Architecture;
	typedef String<32> Version;

	Architecture _architecture  { };

	bool _file_exists(Directory::Path const &path, Rom_label const &file_name)
	{
		if (!_directory_cache.constructed()) {
			error("directory cache is unexpectedly not constructed");
			return false;
		}

		return _directory_cache->file_exists(_depot_dir, path, file_name);
	}

	template <typename FN>
	void _with_file_content(Directory::Path const &path, char const *name, FN const &fn)
	{
		try {
			File_content const content(_heap, Directory(_depot_dir, path),
			                           name, File_content::Limit{16*1024});
			fn(content);
		}
		catch (File_content::Nonexistent_file)   { }
		catch (Directory::Nonexistent_directory) { }
		catch (File::Truncated_during_read)      { }
	}

	/**
	 * Produce report that reflects the query version
	 *
	 * The functor 'fn' is called with an 'Xml_generator &' as argument to
	 * produce the report content.
	 */
	template <typename FN>
	void _gen_versioned_report(Constructible_reporter &reporter, Version const &version,
	                           FN const &fn)
	{
		if (!reporter.constructed())
			return;

		reporter->generate([&] (Xml_generator &xml) {

			if (version.valid())
				xml.attribute("version", version);

			fn(xml);
		});
	}

	Archive::Path _find_rom_in_pkg(File_content const &, Rom_label const &, Recursion_limit);
	void _gen_rom_path_nodes(Xml_generator &, Xml_node const &,
	                         Archive::Path const &, Xml_node const &);
	void _gen_inherited_rom_path_nodes(Xml_generator &, Xml_node const &,
	                                   Archive::Path const &, Recursion_limit);
	void _query_blueprint(Directory::Path const &, Xml_generator &);
	void _collect_source_dependencies(Archive::Path const &, Dependencies &, Recursion_limit);
	void _collect_binary_dependencies(Archive::Path const &, Dependencies &, Recursion_limit);
	void _query_user(Archive::User const &, Xml_generator &);
	void _gen_index_node_rec(Xml_generator &, Xml_node const &, unsigned) const;
	void _gen_index_for_arch(Xml_generator &, Xml_node const &) const;
	void _query_index(Archive::User const &, Archive::Version const &, bool, Xml_generator &);

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		_directory_cache.construct(_heap);

		/*
		 * Depending of the 'query' config attribute, we obtain the query
		 * information from a separate ROM session (attribute value "rom")
		 * or from the depot_querty '<config>'.
		 */
		bool const query_from_rom =
			(config.attribute_value("query", String<5>()) == "rom");

		if (query_from_rom && !_query_rom.constructed()) {
			_query_rom.construct(_env, "query");
			_query_rom->sigh(_query_handler);
		}

		if (!query_from_rom && _query_rom.constructed())
			_query_rom.destruct();

		if (query_from_rom)
			_query_rom->update();

		Xml_node const query = (query_from_rom ? _query_rom->xml() : config);

		_construct_if(query.has_sub_node("scan"),
		              _scan_reporter, _env, "scan", "scan");

		/*
		 * Use 64 KiB as initial report size to avoid the repetitive querying
		 * when successively expanding the reporter.
		 */
		_construct_if(query.has_sub_node("blueprint"),
		              _blueprint_reporter, _env, "blueprint", "blueprint",
		              Expanding_reporter::Initial_buffer_size { 64*1024 });

		_construct_if(query.has_sub_node("dependencies"),
		              _dependencies_reporter, _env, "dependencies", "dependencies");

		_construct_if(query.has_sub_node("user"),
		              _user_reporter, _env, "user", "user");

		_construct_if(query.has_sub_node("index"),
		              _index_reporter, _env, "index", "index");

		_root.apply_config(config.sub_node("vfs"));

		/* ignore incomplete queries that may occur at the startup */
		if (query.has_type("empty"))
			return;

		if (!query.has_attribute("arch"))
			warning("query lacks 'arch' attribute");

		_architecture = query.attribute_value("arch", Architecture());

		Version const version = query.attribute_value("version", Version());

		_gen_versioned_report(_scan_reporter, version, [&] (Xml_generator &xml) {
			query.for_each_sub_node("scan", [&] (Xml_node node) {
				if (node.attribute_value("users", false)) {
					for_each_subdir_name(_heap, _depot_dir, [&] (auto name) {
						xml.node("user", [&] () {
							xml.attribute("name", name); }); }); } }); });

		_gen_versioned_report(_blueprint_reporter, version, [&] (Xml_generator &xml) {
			query.for_each_sub_node("blueprint", [&] (Xml_node node) {
				Archive::Path pkg = node.attribute_value("pkg", Archive::Path());
				try { _query_blueprint(pkg, xml); }
				catch (Xml_generator::Buffer_exceeded) {
					throw; /* handled by 'generate' */ }
				catch (...) {
					xml.node("missing", [&] () {
						xml.attribute("path", pkg); }); }
			});
		});

		_gen_versioned_report(_dependencies_reporter, version, [&] (Xml_generator &xml) {
			Dependencies dependencies(_heap, _depot_dir);
			query.for_each_sub_node("dependencies", [&] (Xml_node node) {

				Archive::Path const path = node.attribute_value("path", Archive::Path());

				if (node.attribute_value("source", false))
					_collect_source_dependencies(path, dependencies, Recursion_limit{8});

				if (node.attribute_value("binary", false))
					_collect_binary_dependencies(path, dependencies, Recursion_limit{8});
			});
			dependencies.xml(xml);
		});

		_gen_versioned_report(_user_reporter, version, [&] (Xml_generator &xml) {

			/* query one user only */
			bool first = true;
			query.for_each_sub_node("user", [&] (Xml_node node) {
				if (!first) return;
				first = false;
				_query_user(node.attribute_value("name", Archive::User()), xml); });
		});

		_gen_versioned_report(_index_reporter, version, [&] (Xml_generator &xml) {
			query.for_each_sub_node("index", [&] (Xml_node node) {
				_query_index(node.attribute_value("user",    Archive::User()),
				             node.attribute_value("version", Archive::Version()),
				             node.attribute_value("content", false),
				             xml); }); });
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


Depot_query::Archive::Path
Depot_query::Main::_find_rom_in_pkg(File_content    const &archives,
                                    Rom_label       const &rom_label,
                                    Recursion_limit        recursion_limit)
{
	Archive::Path result;

	archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {

		/*
		 * \throw Archive::Unknown_archive_type
		 */
		switch (Archive::type(archive_path)) {
		case Archive::SRC:
			{
				Archive::Path const
					rom_path(Archive::user(archive_path), "/bin/",
					         _architecture,               "/",
					         Archive::name(archive_path), "/",
					         Archive::version(archive_path));

				if (_file_exists(rom_path, rom_label))
					result = Archive::Path(rom_path, "/", rom_label);
			}
			break;

		case Archive::RAW:
			{
				Archive::Path const
					rom_path(Archive::user(archive_path), "/raw/",
					         Archive::name(archive_path), "/",
					         Archive::version(archive_path));

				if (_file_exists(rom_path, rom_label))
					result = Archive::Path(rom_path, "/", rom_label);
			}
			break;

		case Archive::PKG:

			_with_file_content(archive_path, "archives", [&] (File_content const &archives) {

				Archive::Path const result_from_pkg =
					_find_rom_in_pkg(archives, rom_label, recursion_limit);

				if (result_from_pkg.valid())
					result = result_from_pkg;
			});
			break;
		}
	});
	return result;
}


void Depot_query::Main::_gen_rom_path_nodes(Xml_generator       &xml,
                                            Xml_node      const &env_xml,
                                            Archive::Path const &pkg_path,
                                            Xml_node      const &runtime)
{
	_with_file_content(pkg_path, "archives", [&] (File_content const &archives) {
		runtime.for_each_sub_node("content", [&] (Xml_node content) {
			content.for_each_sub_node([&] (Xml_node node) {

				/* skip non-rom nodes */
				if (!node.has_type("rom"))
					return;

				Rom_label const label = node.attribute_value("label", Rom_label());

				/* skip ROM that is provided by the environment */
				bool provided_by_env = false;
				env_xml.for_each_sub_node("rom", [&] (Xml_node node) {
					if (node.attribute_value("label", Rom_label()) == label)
						provided_by_env = true; });

				if (provided_by_env) {
					xml.node("rom", [&] () {
						xml.attribute("label", label);
						xml.attribute("env", "yes");
					});
					return;
				}

				Archive::Path const rom_path =
					_find_rom_in_pkg(archives, label, Recursion_limit{8});

				if (rom_path.valid()) {
					xml.node("rom", [&] () {
						xml.attribute("label", label);
						xml.attribute("path", rom_path);
					});

				} else {

					xml.node("missing_rom", [&] () {
						xml.attribute("label", label); });
				}
			});
		});
	});
}


void Depot_query::Main::_gen_inherited_rom_path_nodes(Xml_generator       &xml,
                                                      Xml_node      const &env_xml,
                                                      Archive::Path const &pkg_path,
                                                      Recursion_limit      recursion_limit)
{
	_with_file_content(pkg_path, "archives", [&] (File_content const &archives) {
		archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {

			/* early return if archive path is not a valid pkg path */
			try {
				if (Archive::type(archive_path) != Archive::PKG)
					return;
			}
			catch (Archive::Unknown_archive_type) { return; }

			_with_file_content(archive_path, "runtime" , [&] (File_content const &runtime) {
				runtime.xml([&] (Xml_node node) {
					_gen_rom_path_nodes(xml, env_xml, pkg_path, node); }); });

			_gen_inherited_rom_path_nodes(xml, env_xml, archive_path, recursion_limit);
		});
	});
}


void Depot_query::Main::_query_blueprint(Directory::Path const &pkg_path, Xml_generator &xml)
{
	Directory pkg_dir(_root, Directory::Path("depot/", pkg_path));

	File_content runtime(_heap, pkg_dir, "runtime", File_content::Limit{16*1024});

	runtime.xml([&] (Xml_node node) {

		xml.node("pkg", [&] () {

			xml.attribute("name", Archive::name(pkg_path));
			xml.attribute("path", pkg_path);

			Rom_label const config = node.attribute_value("config", Rom_label());
			if (config.valid())
				xml.attribute("config", config);

			Xml_node const env_xml = _config.xml().has_sub_node("env")
			                       ? _config.xml().sub_node("env") : "<env/>";

			_gen_rom_path_nodes(xml, env_xml, pkg_path, node);

			_gen_inherited_rom_path_nodes(xml, env_xml, pkg_path, Recursion_limit{8});

			String<160> comment("\n\n<!-- content of '", pkg_path, "/runtime' -->\n");
			xml.append(comment.string());
			node.with_raw_node([&] (char const *start, size_t length) {
				xml.append(start, length); });
			xml.append("\n");
		});
	});
}


void Depot_query::Main::_collect_source_dependencies(Archive::Path const &path,
                                                     Dependencies &dependencies,
                                                     Recursion_limit recursion_limit)
{
	try { Archive::type(path); }
	catch (Archive::Unknown_archive_type) {
		warning("archive '", path, "' has unexpected type");
		return;
	}

	dependencies.record(path);

	switch (Archive::type(path)) {

	case Archive::PKG: {
		_with_file_content(path, "archives", [&] (File_content const &archives) {
			archives.for_each_line<Archive::Path>([&] (Archive::Path const &path) {
				_collect_source_dependencies(path, dependencies, recursion_limit); }); });
		break;
	}

	case Archive::SRC: {
		typedef String<160> Api;
		_with_file_content(path, "used_apis", [&] (File_content const &used_apis) {
			used_apis.for_each_line<Archive::Path>([&] (Api const &api) {
				dependencies.record(Archive::Path(Archive::user(path), "/api/", api)); }); });
		break;
	}

	case Archive::RAW:
		break;
	};
}


void Depot_query::Main::_collect_binary_dependencies(Archive::Path const &path,
                                                     Dependencies &dependencies,
                                                     Recursion_limit recursion_limit)
{
	try { Archive::type(path); }
	catch (Archive::Unknown_archive_type) {
		warning("archive '", path, "' has unexpected type");
		return;
	}

	switch (Archive::type(path)) {

	case Archive::PKG:
		dependencies.record(path);

		_with_file_content(path, "archives", [&] (File_content const &archives) {
			archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {
				_collect_binary_dependencies(archive_path, dependencies, recursion_limit); }); });
		break;

	case Archive::SRC:
		dependencies.record(Archive::Path(Archive::user(path), "/bin/",
		                                  _architecture,       "/",
		                                  Archive::name(path), "/",
		                                  Archive::version(path)));
		break;

	case Archive::RAW:
		dependencies.record(path);
		break;
	};
}


void Depot_query::Main::_query_user(Archive::User const &user, Xml_generator &xml)
{
	xml.attribute("name", user);

	try {
		Directory user_dir(_root, Directory::Path("depot/", user));

		File_content download(_heap, user_dir, "download", File_content::Limit{4*1024});
		typedef String<256> Url;
		download.for_each_line<Url>([&] (Url const &url) {
			xml.node("url", [&] () { xml.append_sanitized(url.string()); }); });

		File_content pubkey(_heap, user_dir, "pubkey", File_content::Limit{8*1024});
		xml.node("pubkey", [&] () {
			typedef String<80> Line;
			pubkey.for_each_line<Line>([&] (Line const &line) {
				xml.append_sanitized(line.string());
				xml.append("\n");
			});
		});
	}
	catch (Directory::Nonexistent_file) {
		warning("incomplete depot-user info for '", user, "'"); }

	catch (Directory::Nonexistent_directory) {
		warning("missing depot-user info for '", user, "'"); }
}


void Depot_query::Main::_gen_index_node_rec(Xml_generator  &xml,
                                            Xml_node const &node,
                                            unsigned max_depth) const
{
	if (max_depth == 0) {
		warning("index has too many nesting levels");
		return;
	}

	node.for_each_sub_node([&] (Xml_node const &node) {

		/* check if single index entry is compatible with architecture */
		bool const arch_compatible =
			!node.has_attribute("arch") ||
			(node.attribute_value("arch", Architecture()) == _architecture);

		if (!arch_compatible)
			return;

		if (node.has_type("index")) {
			xml.node("index", [&] () {
				xml.attribute("name", node.attribute_value("name", String<100>()));
				_gen_index_node_rec(xml, node, max_depth - 1);
			});
		}

		if (node.has_type("pkg")) {
			xml.node("pkg", [&] () {
				xml.attribute("path", node.attribute_value("path", Archive::Path()));
				xml.attribute("info", node.attribute_value("info", String<200>()));
			});
		}
	});
};


void Depot_query::Main::_gen_index_for_arch(Xml_generator &xml,
                                            Xml_node const &node) const
{
	/* check of architecture is supported by the index */
	bool supports_arch = false;
	node.for_each_sub_node("supports", [&] (Xml_node const &supports) {
		if (supports.attribute_value("arch", Architecture()) == _architecture)
			supports_arch = true; });

	if (!supports_arch)
		return;

	_gen_index_node_rec(xml, node, 10);
}


void Depot_query::Main::_query_index(Archive::User    const &user,
                                     Archive::Version const &version,
                                     bool const content, Xml_generator &xml)
{
	Directory::Path const index_path("depot/", user, "/index/", version);
	if (!_root.file_exists(index_path)) {
		xml.node("missing", [&] () {
			xml.attribute("user",    user);
			xml.attribute("version", version);
		});
		return;
	}

	xml.node("index", [&] () {
		xml.attribute("user",    user);
		xml.attribute("version", version);

		if (content) {
			try {
				File_content const
					file(_heap, _root, index_path, File_content::Limit{16*1024});

				file.xml([&] (Xml_node node) {
					_gen_index_for_arch(xml, node); });

			} catch (Directory::Nonexistent_file) { }
		}
	});
}


void Component::construct(Genode::Env &env)
{
	static Depot_query::Main main(env);
}


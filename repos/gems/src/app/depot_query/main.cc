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
#include <depot/archive.h>
#include <gems/lru_cache.h>

namespace Depot_query {

	using namespace Depot;

	typedef String<64> Rom_label;

	struct Recursion_limit;
	struct Dependencies;
	class  Stat_cache;
	struct Rom_query;
	class  Cached_rom_query;
	struct Main;
}


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


class Depot_query::Stat_cache
{
	private:

		struct Key
		{
			struct Value
			{
				Archive::Path path;

				bool operator > (Value const &other) const
				{
					return strcmp(path.string(), other.path.string()) > 0;
				}

				bool operator == (Value const &other) const
				{
					return path == other.path;
				}

			} value;
		};

		struct Result { bool file_exists; };

		typedef Lru_cache<Key, Result> Cache;

		Cache::Size const _size;

		Cache _cache;

		Directory const &_dir;

	public:

		Stat_cache(Directory const &dir, Allocator &alloc, Xml_node const config)
		:
			_size({.value = config.attribute_value("stat_cache", Number_of_bytes(64*1024))
			              / Cache::element_size()}),
			_cache(alloc, _size),
			_dir(dir)
		{ }

		bool file_exists(Archive::Path const path)
		{
			/* don't cache the state of the 'local' depot user */
			if (Archive::user(path) == "local")
				return _dir.file_exists(path);

			bool result = false;

			auto hit_fn  = [&] (Result const &cached_result)
			{
				result = cached_result.file_exists;
			};

			auto miss_fn = [&] (Cache::Missing_element &missing_element)
			{
				Result const stat_result { _dir.file_exists(path) };

				/*
				 * Don't cache negative results because files may appear
				 * during installation. Later queries may find files absent
				 * from earlier queries.
				 */
				if (stat_result.file_exists)
					missing_element.construct(stat_result);
			};

			Key const key { .value = { .path = path } };
			(void)_cache.try_apply(key, hit_fn, miss_fn);

			return result;
		}
};


struct Depot_query::Rom_query : Interface
{
	/**
	 * Look up ROM module 'rom_label' in the archives referenced by 'pkg_path'
	 *
	 * \throw Directory::Nonexistent_directory
	 * \throw Directory::Nonexistent_file
	 * \throw File::Truncated_during_read
	 * \throw Recursion_limit::Reached
	 */
	virtual Archive::Path find_rom_in_pkg(Directory::Path const &pkg_path,
	                                      Rom_label       const &rom_label,
	                                      Recursion_limit        recursion_limit) = 0;
};


class Depot_query::Cached_rom_query : public Rom_query
{
	private:

		struct Key
		{
			struct Value
			{
				Archive::Path pkg;
				Rom_label     rom;

				bool operator > (Value const &other) const
				{
					return strcmp(pkg.string(), other.pkg.string()) > 0
					    && strcmp(rom.string(), other.rom.string()) > 0;
				}

				bool operator == (Value const &other) const
				{
					return pkg == other.pkg && rom == other.rom;
				}

			} value;
		};

		typedef Lru_cache<Key, Archive::Path> Cache;

		Cache::Size const _size;

		Cache mutable _cache;

		Rom_query &_rom_query;

	public:

		Cached_rom_query(Rom_query &rom_query, Allocator &alloc, Xml_node const config)
		:
			_size({.value = config.attribute_value("rom_query_cache", Number_of_bytes(64*1024))
			              / Cache::element_size() }),
			_cache(alloc, _size),
			_rom_query(rom_query)
		{ }

		Archive::Path find_rom_in_pkg(Directory::Path const &pkg_path,
		                              Rom_label       const &rom_label,
		                              Recursion_limit        recursion_limit) override
		{
			/* don't cache the state of the 'local' depot user */
			if (Archive::user(pkg_path) == "local")
				return _rom_query.find_rom_in_pkg(pkg_path, rom_label, recursion_limit);

			Archive::Path result { };

			auto hit_fn  = [&] (Archive::Path const &path) { result = path; };

			auto miss_fn = [&] (Cache::Missing_element &missing_element)
			{
				Archive::Path const path =
					_rom_query.find_rom_in_pkg(pkg_path, rom_label, recursion_limit);

				if (path.valid())
					missing_element.construct(path);
			};

			Key const key { .value = { .pkg = pkg_path, .rom = rom_label } };
			(void)_cache.try_apply(key, hit_fn, miss_fn);

			return result;
		}
};


struct Depot_query::Main : private Rom_query
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Constructible<Attached_rom_dataspace> _query_rom { };

	Root_directory _root { _env, _heap, _config.xml().sub_node("vfs") };

	Directory _depot_dir { _root, "depot" };

	Stat_cache _depot_stat_cache { _depot_dir, _heap, _config.xml() };

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

	Cached_rom_query _cached_rom_query { *this, _heap, _config.xml() };

	/**
	 * Rom_query interface
	 */
	Archive::Path find_rom_in_pkg(Directory::Path const &, Rom_label const &,
	                              Recursion_limit) override;

	void _query_blueprint(Directory::Path const &, Xml_generator &);
	void _collect_source_dependencies(Archive::Path const &, Dependencies &, Recursion_limit);
	void _collect_binary_dependencies(Archive::Path const &, Dependencies &, Recursion_limit);
	void _query_user(Archive::User const &, Xml_generator &);
	void _query_index(Archive::User const &, Archive::Version const &, bool, Xml_generator &);

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

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

		_construct_if(query.has_sub_node("blueprint"),
		              _blueprint_reporter, _env, "blueprint", "blueprint");

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
					_depot_dir.for_each_entry([&] (Directory::Entry const &entry) {
						xml.node("user", [&] () {
							xml.attribute("name", entry.name()); }); }); } }); });

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
Depot_query::Main::find_rom_in_pkg(Directory::Path const &pkg_path,
                                   Rom_label       const &rom_label,
                                   Recursion_limit        recursion_limit)
{
	/*
	 * \throw Directory::Nonexistent_directory
	 */
	Directory pkg_dir(_depot_dir, pkg_path);

	/*
	 * \throw Directory::Nonexistent_file
	 * \throw File::Truncated_during_read
	 */
	File_content archives(_heap, pkg_dir, "archives", File_content::Limit{16*1024});

	Archive::Path result;

	archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {

		/*
		 * \throw Archive::Unknown_archive_type
		 */
		switch (Archive::type(archive_path)) {
		case Archive::SRC:
			{
				Archive::Path const
					rom_path(Archive::user(archive_path),    "/bin/",
					         _architecture,                  "/",
					         Archive::name(archive_path),    "/",
					         Archive::version(archive_path), "/", rom_label);

				if (_depot_stat_cache.file_exists(rom_path))
					result = rom_path;
			}
			break;

		case Archive::RAW:
			{
				Archive::Path const
					rom_path(Archive::user(archive_path),    "/raw/",
					         Archive::name(archive_path),    "/",
					         Archive::version(archive_path), "/", rom_label);

				if (_depot_stat_cache.file_exists(rom_path))
					result = rom_path;
			}
			break;

		case Archive::PKG:

			Archive::Path const result_from_pkg =
				_cached_rom_query.find_rom_in_pkg(archive_path, rom_label, recursion_limit);

			if (result_from_pkg.valid())
				result = result_from_pkg;

			break;
		}
	});
	return result;
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

			Xml_node env_xml = _config.xml().has_sub_node("env")
			                 ? _config.xml().sub_node("env") : "<env/>";

			node.for_each_sub_node("content", [&] (Xml_node content) {
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
						_cached_rom_query.find_rom_in_pkg(pkg_path, label, Recursion_limit{8});

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

	try { switch (Archive::type(path)) {

	case Archive::PKG: {
		File_content archives(_heap, Directory(_depot_dir, path),
		                      "archives", File_content::Limit{16*1024});

		archives.for_each_line<Archive::Path>([&] (Archive::Path const &path) {
			_collect_source_dependencies(path, dependencies, recursion_limit); });
		break;
	}

	case Archive::SRC: {
		File_content used_apis(_heap, Directory(_depot_dir, path),
		                       "used_apis", File_content::Limit{16*1024});

		typedef String<160> Api;
		used_apis.for_each_line<Archive::Path>([&] (Api const &api) {
			dependencies.record(Archive::Path(Archive::user(path), "/api/", api));
		});
		break;
	}

	case Archive::RAW:
		break;
	}; }
	catch (File_content::Nonexistent_file) { }
	catch (Directory::Nonexistent_directory) { }
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
		try {
			dependencies.record(path);

			File_content archives(_heap, Directory(_depot_dir, path),
			                      "archives", File_content::Limit{16*1024});

			archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {
				_collect_binary_dependencies(archive_path, dependencies, recursion_limit); });

		}
		catch (File_content::Nonexistent_file) { }
		catch (Directory::Nonexistent_directory) { }
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
					node.with_raw_content([&] (char const *start, size_t lenght) {
						xml.append(start, lenght); }); });

			} catch (Directory::Nonexistent_file) { }
		}
	});
}


void Component::construct(Genode::Env &env)
{
	static Depot_query::Main main(env);
}


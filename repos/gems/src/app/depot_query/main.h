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

#ifndef _MAIN_H_
#define _MAIN_H_

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

	struct Require_verify;
	struct Directory_cache;
	struct Recursion_limit;
	struct Dependencies;
	struct Main;
}


/**
 * Argument type for propagating 'require_verify' query attributes to results
 */
struct Depot_query::Require_verify
{
	bool value;

	static Require_verify from_xml(Xml_node const &node)
	{
		return Require_verify { node.attribute_value("require_verify", true) };
	}

	void gen_attr(Xml_generator &xml) const
	{
		if (!value)
			xml.attribute("require_verify", "no");
	}
};


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

		using Path = Archive::Path;

		struct Collection : Noncopyable
		{
			Allocator &_alloc;

			struct Dependency
			{
				Path           path;
				Require_verify require_verify;

				Dependency(Path const &path, Require_verify require_verify)
				: path(path), require_verify(require_verify) { }

				void gen_attr(Xml_generator &xml) const
				{
					xml.attribute("path", path);
					require_verify.gen_attr(xml);
				}
			};

			using Entry = Registered_no_delete<Dependency>;

			Registry<Entry> _entries { };

			Collection(Allocator &alloc) : _alloc(alloc) { }

			~Collection()
			{
				_entries.for_each([&] (Entry &e) { destroy(_alloc, &e); });
			}

			bool known(Path const &path) const
			{
				bool result = false;
				_entries.for_each([&] (Entry const &entry) {
					if (path == entry.path)
						result = true; });

				return result;
			}

			void insert(Path const &path, Require_verify require_verify)
			{
				if (!known(path))
					new (_alloc) Entry(_entries, path, require_verify);
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

		bool known(Path const &path) const
		{
			return _present.known(path) || _missing.known(path);
		}

		void record(Path const &path, Require_verify require_verify)
		{
			if (_depot.directory_exists(path))
				_present.insert(path, require_verify);
			else
				_missing.insert(path, require_verify);
		}

		void xml(Xml_generator &xml) const
		{
			_present.for_each([&] (Collection::Entry const &entry) {
				xml.node("present", [&] () { entry.gen_attr(xml); }); });

			_missing.for_each([&] (Collection::Entry const &entry) {
				xml.node("missing", [&] () { entry.gen_attr(xml); }); });
		}
};


struct Depot_query::Main
{
	using Url = String<256>;

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
	Constructible_reporter _image_reporter        { };
	Constructible_reporter _image_index_reporter  { };

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
	void _collect_source_dependencies(Archive::Path const &, Dependencies &, Require_verify, Recursion_limit);
	void _collect_binary_dependencies(Archive::Path const &, Dependencies &, Require_verify, Recursion_limit);
	void _scan_user(Archive::User const &, Xml_generator &);
	void _query_user(Archive::User const &, Xml_generator &);
	void _gen_index_node_rec(Xml_generator &, Xml_node const &, unsigned) const;
	void _gen_index_for_arch(Xml_generator &, Xml_node const &) const;
	void _query_index(Archive::User const &, Archive::Version const &, bool, Require_verify, Xml_generator &);
	void _query_image(Archive::User const &, Archive::Name const &, Require_verify, Xml_generator &);
	void _query_image_index(Xml_node const &, Require_verify, Xml_generator &);

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

		/*
		 * Use 64 KiB as initial report size to avoid the repetitive querying
		 * when successively expanding the reporter.
		 */
		_construct_if(query.has_sub_node("blueprint"),
		              _blueprint_reporter, _env, "blueprint", "blueprint",
		              Expanding_reporter::Initial_buffer_size { 64*1024 });

		auto construct_reporter_if_needed = [&] (auto &reporter, auto query_type)
		{
			_construct_if(query.has_sub_node(query_type),
			              reporter, _env, query_type, query_type);
		};

		construct_reporter_if_needed(_scan_reporter,         "scan");
		construct_reporter_if_needed(_dependencies_reporter, "dependencies");
		construct_reporter_if_needed(_user_reporter,         "user");
		construct_reporter_if_needed(_index_reporter,        "index");
		construct_reporter_if_needed(_image_reporter,        "image");
		construct_reporter_if_needed(_image_index_reporter,  "image_index");

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
						_scan_user(name, xml); }); } }); });

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

				Archive::Path  const path = node.attribute_value("path", Archive::Path());
				Require_verify const require_verify = Require_verify::from_xml(node);

				if (node.attribute_value("source", false))
					_collect_source_dependencies(path, dependencies, require_verify,
					                             Recursion_limit{8});

				if (node.attribute_value("binary", false))
					_collect_binary_dependencies(path, dependencies, require_verify,
					                             Recursion_limit{8});
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
				             Require_verify::from_xml(node),
				             xml); }); });

		_gen_versioned_report(_image_reporter, version, [&] (Xml_generator &xml) {
			query.for_each_sub_node("image", [&] (Xml_node node) {
				_query_image(node.attribute_value("user", Archive::User()),
				             node.attribute_value("name", Archive::Name()),
				             Require_verify::from_xml(node),
				             xml); }); });

		_gen_versioned_report(_image_index_reporter, version, [&] (Xml_generator &xml) {
			query.for_each_sub_node("image_index", [&] (Xml_node node) {
				_query_image_index(node, Require_verify::from_xml(node), xml); }); });
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};

#endif /* _MAIN_H_ */

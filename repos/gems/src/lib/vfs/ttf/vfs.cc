/*
 * \brief  Truetype font file system
 * \author Norman Feske
 * \date   2018-03-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>

/* gems includes */
#include <gems/ttf_font.h>
#include <gems/vfs.h>
#include <gems/cached_font.h>

/* local includes */
#include <glyphs_file_system.h>

namespace Vfs_ttf {

	using namespace Vfs;
	using namespace Genode;

	class Font_from_file;
	class Local_factory;
	class File_system;

	struct Dummy_io_response_handler : Vfs::Io_response_handler
	{
		void handle_io_response(Vfs::Vfs_handle::Context *) override { };
	};

	typedef Text_painter::Font Font;
}


struct Vfs_ttf::Font_from_file
{
	typedef Directory::Path Path;

	Directory    const _dir;
	File_content const _content;

	Constructible<Ttf_font const> _font { };

	/*
	 * Each slot of the glyphs file is 64 KiB, which limits the maximum glyph
	 * size to 128x128. We cap the size at 100px to prevent cut-off glyphs.
	 */
	static constexpr float MAX_SIZE_PX = 100.0;

	Font_from_file(Vfs::Env &vfs_env, Path const &file_path, float px)
	:
		_dir(vfs_env),
		_content(vfs_env.alloc(), _dir, file_path, File_content::Limit{10*1024*1024})
	{
		_content.bytes([&] (char const *ptr, size_t) {
			_font.construct(vfs_env.alloc(), ptr, min(px, MAX_SIZE_PX)); });
	}

	Font const &font() const { return *_font; }
};


struct Vfs_ttf::Local_factory : File_system_factory
{
	Font_from_file                       _font;
	Cached_font::Limit                   _cache_limit;
	Cached_font                          _cached_font;
	Glyphs_file_system                   _glyphs_fs;
	Readonly_value_file_system<unsigned> _baseline_fs;
	Readonly_value_file_system<unsigned> _max_width_fs;
	Readonly_value_file_system<unsigned> _max_height_fs;

	Local_factory(Vfs::Env &vfs_env, Xml_node node)
	:
		_font(vfs_env,
		      node.attribute_value("path", Directory::Path()),
		      node.attribute_value("size_px", 16.0)),
		_cache_limit({node.attribute_value("cache", Number_of_bytes())}),
		_cached_font(vfs_env.alloc(), _font.font(), _cache_limit),
		_glyphs_fs    (_cached_font),
		_baseline_fs  ("baseline",   _font.font().baseline()),
		_max_width_fs ("max_width",  _font.font().bounding_box().w()),
		_max_height_fs("max_height", _font.font().bounding_box().h())
	{ }

	Vfs::File_system *create(Vfs::Env&, Genode::Xml_node node) override
	{
		if (node.has_type(Glyphs_file_system::type_name()))
			return &_glyphs_fs;

		if (node.has_type(Readonly_value_file_system<unsigned>::type_name()))
			return _baseline_fs.matches(node)   ? &_baseline_fs
			     : _max_width_fs.matches(node)  ? &_max_width_fs
			     : _max_height_fs.matches(node) ? &_max_height_fs
			     : nullptr;

		return nullptr;
	}
};


class Vfs_ttf::File_system : private Local_factory,
                             private Dummy_io_response_handler,
                             public  Vfs::Dir_file_system
{
	private:

		typedef String<200> Config;
		static Config _config(Xml_node node)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				typedef String<64> Name;
				xml.attribute("name", node.attribute_value("name", Name()));
				xml.node("glyphs", [&] () { });
				xml.node("readonly_value", [&] () { xml.attribute("name", "baseline");   });
				xml.node("readonly_value", [&] () { xml.attribute("name", "max_width");  });
				xml.node("readonly_value", [&] () { xml.attribute("name", "max_height"); });
			});
			return Config(Cstring(buf));
		}

	public:

		File_system(Vfs::Env &vfs_env, Genode::Xml_node node)
		:
			Local_factory(vfs_env, node),
			Vfs::Dir_file_system(vfs_env,
			                     Xml_node(_config(node).string()),
			                     *this)
		{ }

		char const *type() override { return "ttf"; }
};


/**************************
 ** VFS plugin interface **
 **************************/

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &vfs_env,
		                         Genode::Xml_node node) override
		{
			try { return new (vfs_env.alloc())
				Vfs_ttf::File_system(vfs_env, node); }
			catch (...) { }
			return nullptr;
		}
	};

	static Factory factory;
	return &factory;
}

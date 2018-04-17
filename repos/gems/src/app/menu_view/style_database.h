/*
 * \brief  Interface for obtaining widget style information
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STYLE_DATABASE_H_
#define _STYLE_DATABASE_H_

/* gems includes */
#include <gems/file.h>
#include <gems/png_image.h>
#include <gems/cached_font.h>
#include <gems/vfs_font.h>

/* local includes */
#include "types.h"

namespace Menu_view { struct Style_database; }


class Menu_view::Style_database
{
	private:

		enum { PATH_MAX_LEN = 200 };

		typedef String<PATH_MAX_LEN> Path;

		typedef ::File::Reading_failed Reading_failed;

		struct Texture_entry : List<Texture_entry>::Element
		{
			Path             const path;
			::File                 png_file;
			Png_image              png_image;
			Texture<Pixel_rgb888> &texture;

			/**
			 * Constructor
			 *
			 * \throw Reading_failed
			 */
			Texture_entry(Ram_session &ram, Region_map &rm,
			              Allocator &alloc, Path const &path)
			:
				path(path),
				png_file(path.string(), alloc),
				png_image(ram, rm, alloc, png_file.data<void>()),
				texture(*png_image.texture<Pixel_rgb888>())
			{ }
		};

		struct Font_entry : List<Font_entry>::Element
		{
			Path const path;

			Cached_font::Limit _font_cache_limit { 256*1024 };
			Vfs_font           _vfs_font;
			Cached_font        _cached_font;

			Text_painter::Font const &font() const { return _cached_font; }

			/**
			 * Constructor
			 *
			 * \throw Reading_failed
			 */
			Font_entry(Directory const &fonts_dir, Path const &path, Allocator &alloc)
			try :
				path(path),
				_vfs_font(alloc, fonts_dir, path),
				_cached_font(alloc, _vfs_font, _font_cache_limit)
			{ }
			catch (...) { throw Reading_failed(); }
		};

		Ram_session     &_ram;
		Region_map      &_rm;
		Allocator       &_alloc;
		Directory const &_fonts_dir;

		/*
		 * The list is mutable because it is populated as a side effect of
		 * calling the const lookup function.
		 */
		List<Texture_entry> mutable _textures;
		List<Font_entry>    mutable _fonts;

		template <typename T>
		T const *_lookup(List<T> &list, char const *path) const
		{
			for (T const *e = list.first(); e; e = e->next())
				if (Genode::strcmp(e->path.string(), path) == 0)
					return e;

			return 0;
		}

		/*
		 * Assemble path name 'styles/<widget>/<style>/<name>.<extension>'
		 */
		static Path _construct_path(Xml_node node, char const *name,
		                            char const *extension)
		{
			typedef String<64> Style;
			Style const style = node.attribute_value("style", Style("default"));

			return Path("/styles/", node.type(), "/", style, "/", name, ".", extension);
		}

	public:

		Style_database(Ram_session &ram, Region_map &rm, Allocator &alloc,
		               Directory const &fonts_dir)
		:
			_ram(ram), _rm(rm), _alloc(alloc), _fonts_dir(fonts_dir)
		{ }

		Texture<Pixel_rgb888> const *texture(Xml_node node, char const *png_name) const
		{
			Path const path = _construct_path(node, png_name, "png");

			if (Texture_entry const *e = _lookup(_textures, path.string()))
				return &e->texture;

			/*
			 * Load and remember PNG image
			 */
			try {
				Texture_entry *e = new (_alloc)
					Texture_entry(_ram, _rm, _alloc, path.string());

				_textures.insert(e);
				return &e->texture;

			} catch (Reading_failed) {
				warning("could not read texture data from file \"", path.string(), "\"");
				return nullptr;
			}

			return nullptr;
		}

		Text_painter::Font const *font(Xml_node node) const
		{
			Path const path = node.attribute_value("font", Path("text/regular"));
			if (Font_entry const *e = _lookup(_fonts, path.string()))
				return &e->font();

			/*
			 * Load and remember font
			 */
			try {
				Font_entry *e = new (_alloc)
					Font_entry(_fonts_dir, path, _alloc);

				_fonts.insert(e);
				return &e->font();

			} catch (Reading_failed) {

				warning("could not read font from file \"", path.string(), "\"");
				return nullptr;
			}

			return nullptr;
		}
};

#endif /* _STYLE_DATABASE_H_ */

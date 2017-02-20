/*
 * \brief  Menu view
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

/* local includes */
#include "types.h"

namespace Menu_view { struct Style_database; }


class Menu_view::Style_database
{
	private:

		enum { PATH_MAX_LEN = 200 };

		struct Texture_entry : List<Texture_entry>::Element
		{
			String<PATH_MAX_LEN>   path;
			File                   png_file;
			Png_image              png_image;
			Texture<Pixel_rgb888> &texture;

			/**
			 * Constructor
			 *
			 * \throw Reading_failed
			 */
			Texture_entry(Ram_session &ram, Region_map &rm,
			              Allocator &alloc, char const *path)
			:
				path(path),
				png_file(path, alloc),
				png_image(ram, rm, alloc, png_file.data<void>()),
				texture(*png_image.texture<Pixel_rgb888>())
			{ }
		};

		struct Font_entry : List<Font_entry>::Element
		{
			String<PATH_MAX_LEN> path;
			File                 tff_file;
			Text_painter::Font   font;

			/**
			 * Constructor
			 *
			 * \throw Reading_failed
			 */
			Font_entry(char const *path, Allocator &alloc)
			:
				path(path),
				tff_file(path, alloc),
				font(tff_file.data<char>())
			{ }
		};

		Ram_session &_ram;
		Region_map  &_rm;
		Allocator   &_alloc;

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

		typedef String<256> Path;

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

		Style_database(Ram_session &ram, Region_map &rm, Allocator &alloc)
		:
			_ram(ram), _rm(rm), _alloc(alloc)
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

			} catch (File::Reading_failed) {

				warning("could not read texture data from file \"", path.string(), "\"");
				return nullptr;
			}

			return nullptr;
		}

		Text_painter::Font const *font(Xml_node node, char const *tff_name) const
		{
			Path const path = _construct_path(node, tff_name, "tff");

			if (Font_entry const *e = _lookup(_fonts, path.string()))
				return &e->font;

			/*
			 * Load and remember font
			 */
			try {
				Font_entry *e = new (_alloc)
					Font_entry(path.string(), _alloc);

				_fonts.insert(e);
				return &e->font;

			} catch (File::Reading_failed) {

				warning("could not read font from file \"", path.string(), "\"");
				return nullptr;
			}

			return nullptr;
		}
};

#endif /* _STYLE_DATABASE_H_ */

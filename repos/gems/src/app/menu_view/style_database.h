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

namespace Menu_view {

	struct Label_style;
	struct Style_database;
}


struct Menu_view::Label_style
{
	Color color;
};


class Menu_view::Style_database
{
	public:

		struct Changed_handler : Interface
		{
			virtual void handle_style_changed() = 0;
		};

	private:

		enum { PATH_MAX_LEN = 200 };

		/*
		 * True whenever the style must be updated, e.g., because the font size
		 * changed. The member is marked as 'mutable' because it must be
		 * writeable by the 'Font_entry'.
		 */
		bool mutable _out_of_date = false;

		typedef String<PATH_MAX_LEN> Path;

		typedef ::File::Reading_failed Reading_failed;

		struct Label_style_entry : List<Label_style_entry>::Element, Noncopyable
		{
			Path        const path;  /* needed for lookup */
			Label_style const style;

			bool const out_of_date = false;

			static Label_style _init_style(Allocator &alloc,
			                               Directory const &styles_dir,
			                               Path const &path)
			{
				Label_style result { .color = Color(0, 0, 0) };

				try {
					File_content const content(alloc, styles_dir, path,
					                           File_content::Limit{1024});
					content.xml([&] (Xml_node node) {
						result.color = node.attribute_value("color", result.color);
					});
				} catch (...) { }

				return result;
			}

			Label_style_entry(Allocator &alloc, Directory const &styles_dir,
			                  Path const &path)
			:
				path(path), style(_init_style(alloc, styles_dir, path))
			{ }
		};

		struct Texture_entry : List<Texture_entry>::Element
		{
			Path             const path;
			File_content           png_file;
			Png_image              png_image;
			Texture<Pixel_rgb888> &texture;

			bool const out_of_date = false;

			void const *_png_data()
			{
				void const *result = nullptr;
				png_file.bytes([&] (char const *ptr, size_t) { result = ptr; });
				return result;
			}

			/**
			 * Constructor
			 *
			 * \throw Reading_failed
			 */
			Texture_entry(Ram_allocator &ram, Region_map &rm, Allocator &alloc,
			              Directory const &dir, Path const &path)
			:
				path(path),
				png_file(alloc, dir, path.string(), File_content::Limit{256*1024}),
				png_image(ram, rm, alloc, _png_data()),
				texture(*png_image.texture<Pixel_rgb888>())
			{ }
		};

		struct Font_entry : List<Font_entry>::Element
		{
			Path const path;

			bool out_of_date = false;

			Style_database const &_style_database;

			Cached_font::Limit _font_cache_limit { 1024*1024 };
			Vfs_font           _vfs_font;
			Cached_font        _cached_font;

			Watch_handler<Font_entry> _glyphs_changed_handler;

			void _handle_glyphs_changed()
			{
				out_of_date = true;
				_style_database._out_of_date = true;

				/* schedule dialog redraw */
				Signal_transmitter(_style_database._style_changed_sigh).submit();
			}

			Text_painter::Font const &font() const { return _cached_font; }

			/**
			 * Constructor
			 *
			 * \throw Reading_failed
			 */
			Font_entry(Entrypoint &ep, Directory const &fonts_dir,
			           Path const &path, Allocator &alloc,
			           Style_database const &style_database)
			try :
				path(path),
				_style_database(style_database),
				_vfs_font(alloc, fonts_dir, path),
				_cached_font(alloc, _vfs_font, _font_cache_limit),
				_glyphs_changed_handler(ep, fonts_dir, Path(path, "/glyphs"),
				                        *this, &Font_entry::_handle_glyphs_changed)
			{ }
			catch (...) { throw Reading_failed(); }
		};

		Entrypoint      &_ep;
		Ram_allocator   &_ram;
		Region_map      &_rm;
		Allocator       &_alloc;
		Directory const &_fonts_dir;
		Directory const &_styles_dir;

		Signal_context_capability _style_changed_sigh;

		/*
		 * The lists are mutable because they are populated as a side effect of
		 * calling the const lookup functions.
		 */
		List<Texture_entry>     mutable _textures     { };
		List<Font_entry>        mutable _fonts        { };
		List<Label_style_entry> mutable _label_styles { };

		template <typename T>
		T const *_lookup(List<T> &list, char const *path) const
		{
			for (T const *e = list.first(); e; e = e->next())
				if ((Genode::strcmp(e->path.string(), path) == 0) && !e->out_of_date)
					return e;

			return nullptr;
		}

		/*
		 * Assemble path name 'styles/<widget>/<style>/<name>.png'
		 */
		static Path _construct_png_path(Xml_node node, char const *name)
		{
			typedef String<64> Style;
			Style const style = node.attribute_value("style", Style("default"));

			return Path(node.type(), "/", style, "/", name, ".png");
		}

		/*
		 * Assemble path of style file relative to the styles directory
		 */
		static Path _widget_style_path(Xml_node const &node)
		{
			typedef String<64> Style;
			Style const style = node.attribute_value("style", Style("default"));

			return Path(node.type(), "/", style, "/", "style");
		}

		Label_style const &_label_style(Xml_node node) const
		{
			Path const path = _widget_style_path(node);

			if (Label_style_entry const *e = _lookup(_label_styles, path.string()))
				return e->style;

			/*
			 * Load and remember style
			 */
			Label_style_entry &e = *new (_alloc)
				Label_style_entry(_alloc, _styles_dir, path);

			_label_styles.insert(&e);
			return e.style;
		}

	public:

		Style_database(Entrypoint &ep, Ram_allocator &ram, Region_map &rm,
		               Allocator &alloc,
		               Directory const &fonts_dir, Directory const &styles_dir,
		               Signal_context_capability style_changed_sigh)
		:
			_ep(ep), _ram(ram), _rm(rm), _alloc(alloc),
			_fonts_dir(fonts_dir), _styles_dir(styles_dir),
			_style_changed_sigh(style_changed_sigh)
		{ }

		Texture<Pixel_rgb888> const *texture(Xml_node node, char const *png_name) const
		{
			Path const path = _construct_png_path(node, png_name);

			if (Texture_entry const *e = _lookup(_textures, path.string()))
				return &e->texture;

			/*
			 * Load and remember PNG image
			 */
			try {
				Texture_entry *e = new (_alloc)
					Texture_entry(_ram, _rm, _alloc, _styles_dir, path.string());

				_textures.insert(e);
				return &e->texture;

			} catch (...) {
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
					Font_entry(_ep, _fonts_dir, path, _alloc, *this);

				_fonts.insert(e);
				return &e->font();

			} catch (Reading_failed) {

				warning("could not read font from file \"", path.string(), "\"");
				return nullptr;
			}

			return nullptr;
		}

		template <typename FN>
		void with_label_style(Xml_node node, FN const &fn) const
		{
			fn(_label_style(node));
		}

		void flush_outdated_styles()
		{
			if (!_out_of_date)
				return;

			/* flush fonts that are marked as out of date */
			for (Font_entry *font = _fonts.first(), *next = nullptr; font; ) {
				next = font->next();

				if (font->out_of_date) {
					_fonts.remove(font);
					destroy(_alloc, font);
				}
				font = next;
			}
			_out_of_date = false;
		}

		bool up_to_date() const { return !_out_of_date; }
};

#endif /* _STYLE_DATABASE_H_ */
